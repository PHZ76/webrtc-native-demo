#pragma once

#include "net/EventLoop.h"

class UdpConnection
{
public:
	UdpConnection(std::shared_ptr<xop::EventLoop> event_loop);
	virtual ~UdpConnection();

	bool Init(std::string ip, uint16_t port);
	void Destroy();

protected:
	void OnRecv();
	virtual void OnRecv(uint8_t* pkt, size_t size);
	virtual int  OnSend(uint8_t* pkt, size_t size);

	std::shared_ptr<xop::EventLoop> event_loop_;
	std::shared_ptr<xop::Channel> channel_;
	SOCKET socket_ = 0;
	std::string local_ip_;
	uint16_t local_port_ = 0;
	sockaddr_in peer_addr_ = {};
};

