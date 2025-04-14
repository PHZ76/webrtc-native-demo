#include "udp_connection.h"
#include "net/SocketUtil.h"

UdpConnection::UdpConnection(std::shared_ptr<xop::EventLoop> event_loop)
	: event_loop_(event_loop)
{

}

UdpConnection::~UdpConnection()
{

}

bool UdpConnection::Init(std::string ip, uint16_t port)
{
	socket_ = ::socket(AF_INET, SOCK_DGRAM, 0);
	xop::SocketUtil::SetSendBufSize(socket_, 50 * 1024);
	if (!xop::SocketUtil::Bind(socket_, ip, port)) {
		xop::SocketUtil::Close(socket_);
		socket_ = 0;
		return false;
	}

	channel_.reset(new xop::Channel(socket_));
	channel_->SetReadCallback([this]() { this->OnRecv(); });
	channel_->EnableReading();
	event_loop_->UpdateChannel(channel_);

	return true;
}

void UdpConnection::Destroy()
{
	if (channel_ && event_loop_) {
		event_loop_->RemoveChannel(channel_);
		channel_.reset();
	}

	if (socket_) {
		xop::SocketUtil::Close(socket_);
		socket_ = 0;
	}
}

void UdpConnection::OnRecv()
{
	sockaddr_in peer_addr = {};
	socklen_t addr_len = sizeof(sockaddr_in);

	uint8_t buf[1500] = { 0 };
	int recv_bytes = recvfrom(socket_, (char*)buf, sizeof(buf), 0, (sockaddr*)&peer_addr, &addr_len);
	memcpy_s(&peer_addr_, sizeof(sockaddr_in), &peer_addr, sizeof(sockaddr_in));

	OnRecv(buf, recv_bytes);
}

void UdpConnection::OnRecv(uint8_t* buf, size_t recv_bytes)
{

}

int UdpConnection::OnSend(uint8_t* pkt, size_t size)
{
	int sent_bytes = sendto(socket_, (char*)pkt, (int)size, 0, (sockaddr*)&peer_addr_, sizeof(struct sockaddr_in));
	if (sent_bytes <= 0) {
		if (EAGAIN == errno) {
			sent_bytes = 0;
		}
	}

	return sent_bytes;
}