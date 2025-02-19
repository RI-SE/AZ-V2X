#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <set>
#include <proton/connection.hpp>
#include <proton/container.hpp>
#include <proton/message.hpp>
#include <proton/messaging_handler.hpp>
#include <crow.h>
#include "denm_message.hpp"
#include "amqp_client.hpp"

class DenmService {
public:
    DenmService(
        const std::string& amqp_url,
        const std::string& amqp_send_address,
        const std::string& amqp_receive_address,
        const std::string& http_host,
        int http_port,
        int ws_port,
        bool receiver_mode,
        bool sender_mode
    );
    
    ~DenmService();
    
    void start();
    void stop();

private:
    void handleDenmPost(const crow::request& req, crow::response& res);
    DenmMessage createDenmFromJson(const crow::json::rvalue& j);
    void setupRoutes();

    void broadcastMessage(const std::string& message);
    void runReceiverLoop();

    class AmqpHandler : public proton::messaging_handler {
    public:
        AmqpHandler(DenmService& service);
        void on_message(proton::delivery&, proton::message&) override;
        
        void on_sender_open(proton::sender& s) override {
            amqp_sender_ = &s;
        }
        
        proton::sender* amqp_sender_{nullptr};
    private:
        DenmService& service_;
    };

    void run_amqp_container();
    void run_http_server();
    void broadcast_denm(const crow::json::rvalue& denm_message);

    std::string amqp_url_;
    std::string amqp_send_address_;
    std::string amqp_receive_address_;
    std::string http_host_;
    int http_port_;
    int ws_port_;
    bool receiver_mode_;
    bool sender_mode_;

    std::atomic<bool> running_{false};
    std::unique_ptr<proton::container> amqp_container_;
    std::unique_ptr<AmqpHandler> amqp_handler_;
    
    crow::App<> app_;  // Crow application instance
    std::mutex ws_connections_mutex_;
    // Container of active websocket connections
    std::set<crow::websocket::connection*> ws_connections_;

    std::thread amqp_thread_;
    std::thread http_thread_;
    std::thread ws_thread_;
    std::thread amqp_receiver_thread_;
    std::thread container_thread_;

    std::unique_ptr<sender> amqp_sender_;
    std::unique_ptr<receiver> amqp_receiver_;

}; 