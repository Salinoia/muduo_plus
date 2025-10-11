#include "MQClient.h"

MQClient::MQClient(EventLoop* loop, const std::string& url) : loop_(loop) {
    handler_ = std::make_unique<MQHandler>(loop_);
    AMQP::Address address(url);
    connection_ = std::make_unique<AMQP::TcpConnection>(handler_.get(), address);
}

MQClient::~MQClient() {
    if (!handler_ || !connection_)
        return;

    auto h = std::move(handler_);
    auto c = std::move(connection_);
    auto handler = std::shared_ptr<MQHandler>(std::move(h));
    auto conn = std::shared_ptr<AMQP::TcpConnection>(std::move(c));

    loop_->runInLoop([handler, conn]() {
        conn->close();
        handler->unregister();
    });
}
