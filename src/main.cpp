#include "amqp_client.hpp"
#include "ssl_utils.hpp"
#include "denm_message.hpp"
#include <proton/container.hpp>
#include <proton/message.hpp>
#include <iostream>
#include <thread>
#include <string>
#include <spdlog/spdlog.h>
#include <sstream>
#include <iomanip>

int main(int argc, char **argv) {
    try {
        // Set certificate directory
        if (argc > 1) {
            set_cert_directory(argv[1]);
        } else {
            set_cert_directory("ssl-certs/");
        }
        
        // Create and configure DENM message
        DenmMessage denm;
        
        // Set management container values
        denm.setActionId(12345);
        denm.setDetectionTime(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
        denm.setReferenceTime(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
        denm.setEventPosition(52.5200, 13.4050, 30.0); // Example: Berlin coordinates
        denm.setRelevanceDistance(RelevanceDistance_lessThan100m);
        denm.setValidityDuration(std::chrono::seconds(300)); // 5 minutes

        // Set situation container values
        denm.setInformationQuality(3);
        denm.setCauseCode(CauseCodeType_roadworks);
        denm.setSubCauseCode(1);
        denm.setEventSpeed(13.89);
        denm.setEventHeading(3601);

        // Set location container values
        denm.setEventPositionConfidence(95);
        denm.setEventHeadingConfidence(98);

        // Log DENM message details
        spdlog::info("Created DENM message with following details:");
        spdlog::info("Management Container:");
        spdlog::info("  Action ID: {}", denm.getActionId());
        spdlog::info("  Detection Time: {}", denm.getDetectionTimeFormatted());
        spdlog::info("  Reference Time: {}", denm.getReferenceTimeFormatted());
        spdlog::info("  Validity Duration: {}s", denm.getValidityDuration().count());
        
        spdlog::info("Situation Container:");
        spdlog::info("  Cause Code: {}", denm.getCauseCode());
        spdlog::info("  Sub Cause Code: {}", denm.getSubCauseCode());
        spdlog::info("  Information Quality: {}", denm.getInformationQuality());
        spdlog::info("  Event Speed: {:.2f} m/s", denm.getEventSpeed());
        spdlog::info("  Event Heading: {:.2f} degrees", denm.getEventHeading());

        spdlog::info("Location Container:");
        spdlog::info("  Position Confidence: {:.2f}", denm.getEventPositionConfidence());
        spdlog::info("  Heading Confidence: {:.2f}", denm.getEventHeadingConfidence());
        spdlog::info("  Speed Confidence: {:.2f}", denm.getEventSpeedConfidence());

        // Build the DENM message in a UPER encoded format
        auto denmMessage = denm.buildDenm();
        auto encodedMessage = denmMessage.encode();
        std::string encodedMessageString(encodedMessage.begin(), encodedMessage.end());
        
        // Convert to hex string for logging
        std::stringstream hexStream;
        for (unsigned char byte : encodedMessage) {
            hexStream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
        }
        spdlog::info("UPER encoded DENM message (hex): {}", hexStream.str());

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
        spdlog::error("Error: {}", e.what());
    }
    return 1;
}