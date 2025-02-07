#ifndef SERVER_HANDLER_HPP
#define SERVER_HANDLER_HPP

#include <proton/messaging_handler.hpp>
#include <proton/listener.hpp>
#include <proton/message.hpp>
#include <proton/sender.hpp> 


struct server_handler : public proton::messaging_handler {
    proton::listener listener;
    proton::message pending_response_;
    bool has_pending_response_ = false;

    void on_connection_open(proton::connection &c) override;
    void on_message(proton::delivery &d, proton::message &m) override;
    void on_sender_open(proton::sender& s) override;
    void on_sendable(proton::sender& s) override;
    void on_sender_error(proton::sender& s) override;
    void send_pending_response();

private:
    proton::sender sender_;
};

#endif // SERVER_HANDLER_HPP 