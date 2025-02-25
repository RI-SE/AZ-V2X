#include "interchange_service.hpp"
#include "denm_message.hpp"
#include <spdlog/spdlog.h>

InterchangeService::InterchangeService(const std::string& amqp_url,
                                     const std::string& amqp_send_address,
                                     const std::string& amqp_receive_address) :
    amqp_url_(amqp_url),
    amqp_send_address_(amqp_send_address),
    amqp_receive_address_(amqp_receive_address),
    amqp_container_(std::make_unique<proton::container>()) {
    
    // Subscribe to outgoing DENM events
    EventBus::getInstance().subscribe("denm.outgoing", 
        [this](const nlohmann::json& denm) {
            this->handleOutgoingDenm(denm);
        });
}

void InterchangeService::start() {
    if (running_) return;
    running_ = true;

    // Start AMQP container
    container_thread_ = std::thread([this]() {
        try {
            amqp_container_->run();
        } catch (const std::exception& e) {
            spdlog::error("AMQP container error: {}", e.what());
        }
    });

    // Setup sender
    amqp_sender_ = std::make_unique<sender>(
        *amqp_container_, amqp_url_, amqp_send_address_, "denm-sender");

    // Setup receiver
    setupAmqpReceiver();
}

void InterchangeService::setupAmqpReceiver() {
    amqp_receiver_ = std::make_unique<receiver>(
        *amqp_container_, amqp_url_, amqp_receive_address_, "denm-receiver");

    receiver_thread_ = std::thread([this]() {
        while (running_) {
            try {
                proton::message msg = amqp_receiver_->receive();
                if (msg.body().type() == proton::BINARY) {
                    auto data = proton::get<proton::binary>(msg.body());
                    DenmMessage denm;
                    denm.fromUper(data);
                    
                    // Publish received DENM to event bus
                    EventBus::getInstance().publish("denm.incoming", denm.toJson());
                }
            } catch (const std::exception& e) {
                if (running_) {
                    spdlog::error("AMQP receiver error: {}", e.what());
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
        }
    });
}

void InterchangeService::handleOutgoingDenm(const nlohmann::json& denm) {
    try {
        // Create and encode DENM message
        DenmMessage denmMsg = DenmMessage::fromJson(denm);
        auto encoded = denmMsg.getUperEncoded();

        // Create and send AMQP message
        proton::message amqp_msg;
        amqp_msg.body(proton::binary(encoded.begin(), encoded.end()));
        // Set other AMQP properties as before...
        
        amqp_sender_->send(amqp_msg);
    } catch (const std::exception& e) {
        spdlog::error("Failed to send DENM: {}", e.what());
    }
}

void InterchangeService::stop() {
    if (!running_) return;
    running_ = false;
    
    amqp_container_->stop();
    
    if (amqp_sender_) amqp_sender_->close();
    if (amqp_receiver_) amqp_receiver_->close();
    
    if (receiver_thread_.joinable()) receiver_thread_.join();
    if (container_thread_.joinable()) container_thread_.join();
} 