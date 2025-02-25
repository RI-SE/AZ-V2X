#pragma once

#include "amqp_client.hpp"
#include "event_bus.hpp"
#include "ssl_utils.hpp"
#include <memory>
#include <thread>
#include <atomic>
#include <string>
#include <nlohmann/json.hpp>

class InterchangeService {
public:
    InterchangeService(const std::string& amqp_url,
                      const std::string& amqp_send_address,
                      const std::string& amqp_receive_address,
                      const std::string& cert_dir);
    
    void start();
    void stop();

private:
    void handleOutgoingDenm(const nlohmann::json& denm);
    void setupAmqpReceiver();
    void setupContainerOptions();
    
    std::string amqp_url_;
    std::string amqp_send_address_;
    std::string amqp_receive_address_;
    std::string cert_dir_;

    std::unique_ptr<proton::container> amqp_container_;
    std::unique_ptr<sender> amqp_sender_;
    std::unique_ptr<receiver> amqp_receiver_;
    
    std::thread container_thread_;
    std::thread receiver_thread_;
    std::atomic<bool> running_{false};
}; 