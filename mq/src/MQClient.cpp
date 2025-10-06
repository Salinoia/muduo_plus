#include "MQClient.h"

MQClient::MQClient(EventLoop* loop, const std::string& url) : loop_(loop) {
    handler_ = std::make_unique<MQHandler>(loop_);
    AMQP::Address address(url);
    connection_ = std::make_unique<AMQP::TcpConnection>(handler_.get(), address);
}

MQClient::~MQClient() {
    if (connection_) {
        connection_->close();
    }
}
