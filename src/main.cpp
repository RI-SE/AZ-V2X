#include "amqp_client.hpp"
#include "ssl_utils.hpp"
#include <proton/container.hpp>
#include <proton/message.hpp>
#include <iostream>
#include <thread>
#include <string>
#include <spdlog/spdlog.h>

int main(int argc, char **argv) {
    try {
        // Set certificate directory
        if (argc > 1) {
            set_cert_directory(argv[1]);
        } else {
            set_cert_directory("ssl-certs/");
        }
        
        // Create a proton container
        proton::container container;
        
        // Create URL and address
        std::string url("amqp://localhost:5672");
        std::string address("examples");
        spdlog::info("Connecting to AMQP server at {}", url);

        // Start container thread
        auto container_thread = std::thread([&]() { container.run(); });

        // Create sender and receiver
        receiver recv(container, url, address);
        sender send(container, url, address);
        
        // Small delay to ensure connections are established
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); 
        
        // Send a message
        proton::message msg("Hello, AMQP!");
        spdlog::info("Sending message to: {}", address);
        send.send(msg);

        // Wait for response with timeout
        spdlog::info("Waiting for message on: {}", address);
        auto start_time = std::chrono::steady_clock::now();
        bool received_response = false;
        while (!received_response) {
            try {
                auto received = recv.receive();
                if (received.body().empty()) {
                    spdlog::info("Received empty message");
                    continue;
                }
                spdlog::info("Received message: {}", proton::get<std::string>(received.body()));
                received_response = true;
            } catch (const std::exception& e) {
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count() > 5) {
                    spdlog::error("Timeout waiting for response");
                    spdlog::error("Last error: {}", e.what());
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        // Cleanup
        send.close();
        recv.close();
        container_thread.join();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 1;
}