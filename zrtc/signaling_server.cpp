#include "signaling_server.h"

#include <iostream>
#include <fstream>
#include <sstream>

#include "rtc/rtc_log.h"

// /nlohmann/json
#include "json/json.hpp"

static const char* kRequestPaths[] = {
	"/api/fetchoffer",
	"/api/sendanswer",
	"/api/stop",
};

enum RequestPathIndex {
	kFetchOffer,
	kSendAnswer,
	kStop,
};

static std::string dump_headers(const httplib::Headers& headers) 
{
	std::string s;
	char buf[BUFSIZ];

	for (auto it = headers.begin(); it != headers.end(); ++it) {
		const auto& x = *it;

		snprintf(buf, sizeof(buf), "%s: %s\n", x.first.c_str(), x.second.c_str());
		s += buf;
	}

	return s;
}

static  std::string http_log(const httplib::Request& req, const httplib::Response& res) 
{
	std::string s;
	char buf[BUFSIZ];

	s += "================================\n";

	snprintf(buf, sizeof(buf), "%s %s %s", req.method.c_str(),
		req.version.c_str(), req.path.c_str());
	s += buf;

	std::string query;
	for (auto it = req.params.begin(); it != req.params.end(); ++it) {
		const auto& x = *it;
		snprintf(buf, sizeof(buf), "%c%s=%s",
			(it == req.params.begin()) ? '?' : '&', x.first.c_str(),
			x.second.c_str());
		query += buf;
	}
	snprintf(buf, sizeof(buf), "%s\n", query.c_str());
	s += buf;

	s += dump_headers(req.headers);

	s += "--------------------------------\n";

	snprintf(buf, sizeof(buf), "%d %s\n", res.status, res.version.c_str());
	s += buf;
	s += dump_headers(res.headers);
	s += "\n";

	if (!res.body.empty()) { s += res.body; }

	s += "\n";

	return s;
}

static std::vector<std::string> split(const std::string& s, char delimiter) {
	std::vector<std::string> tokens;
	std::string token;
	std::istringstream tokenStream(s);
	while (std::getline(tokenStream, token, delimiter)) {
		tokens.push_back(token);
	}
	return tokens;
}

static std::string render_template(const std::string& template_path, const std::map<std::string, std::string>& context)
{
	std::ifstream file(template_path);
	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string content = buffer.str();

	for (auto& pair : context) {
		std::string key = "{{" + pair.first + "}}";
		size_t pos = 0;
		while ((pos = content.find(key, pos)) != std::string::npos) {
			content.replace(pos, key.length(), pair.second);
			pos += pair.second.length();
		}
	}

	return content;
}

SignalingServer::SignalingServer()
{

}
SignalingServer::~SignalingServer()
{
	Destroy();
}

bool SignalingServer::Init(const SignalingConfig& config, std::shared_ptr<SignalingHandler> handler)
{
	signaling_config_ = config;
	signaling_handler_ = handler;

	if (!config.cert_path.empty() && !config.key_path.empty()) {
		server_ = std::make_shared<httplib::SSLServer>(config.cert_path.c_str(), config.key_path.c_str());
		if (!server_->is_valid()) {
			RTC_LOG_INFO("init ssl server failed.");
			server_.reset();
		}
		else {
			RTC_LOG_INFO("init ssl server succeed.");
		}
	}

	if (!server_) {
		server_ = std::make_shared<httplib::Server>();
		if (!server_->is_valid()) {
			RTC_LOG_INFO("init server failed.");
			return false;
		}
	}

	// Mount / to ./www directory
	auto ret = server_->set_mount_point("/", "./static");
	if (!ret) {
		// The specified base directory doesn't exist...
	}

	server_->Post(kRequestPaths[kFetchOffer], [this](const httplib::Request& req, httplib::Response& res) {
		std::string uid, stream_name;
		nlohmann::json response;
		response["status"] = -1;
		response["offer"] = "{}";

		for (auto it = req.params.begin(); it != req.params.end(); ++it) {
			const auto& param = *it;
			if(param.first == "uid") {
				uid = param.second;
			}
			else if (param.first == "stream") {
				stream_name = param.second;
			}
		}
		if (!uid.empty() && signaling_handler_) {
			std::string offer;
			signaling_handler_->GetLocalDescription(uid, offer);
			response["status"] = 0;
			response["offer"] = offer;
		}
		res.set_content(response.dump(), "application/json");
	});

	server_->Post(kRequestPaths[kSendAnswer], [this](const httplib::Request& req, httplib::Response& res) {
		std::string uid, stream_name, answer;
		nlohmann::json response;
		response["status"] = -1;

		for (auto it = req.params.begin(); it != req.params.end(); ++it) {
			const auto& param = *it;
			if (param.first == "uid") {
				uid = param.second;
			}
			else if (param.first == "stream") {
				stream_name = param.second;
			}
			else if (param.first == "answer") {
				answer = param.second;				
			}
		}

		if (!uid.empty() && signaling_handler_) {
			signaling_handler_->OnRemoteDescription(uid, answer);
			response["status"] = 0;
		}

		res.set_content(response.dump(), "application/json");
	});

	server_->Get("/rtc", [this](const httplib::Request& req, httplib::Response& res) {
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> dis(1, INT_MAX);
		std::map<std::string, std::string> context = {
			{"host", signaling_config_.host},
			{"uid", std::to_string(static_cast<int>(dis(gen)))},
		};

		std::string html = render_template("./static/rtc_template.html", context);
		res.set_content(html, "text/html");
	});

	server_->set_error_handler([](const httplib::Request& req, httplib::Response& res) {
		const char* fmt = "<p>Error Status: <span style='color:red;'>%d</span></p>";
		char buf[BUFSIZ];
		snprintf(buf, sizeof(buf), fmt, res.status);
		res.set_content(buf, "text/html");
	});

	server_->set_logger([](const httplib::Request& req, const httplib::Response& res) {
		//printf("%s", http_log(req, res).c_str());
	});

	poll_thread_.reset(new std::thread([this] {
		server_->listen(signaling_config_.host.c_str(), signaling_config_.port);
	}));
	
	return true;
}

void SignalingServer::Destroy()
{
	if (poll_thread_ && server_) {
		server_->stop();
		poll_thread_->join();
		poll_thread_.reset();
	}
}
