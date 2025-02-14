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
#include <boost/program_options.hpp>

DenmMessage createDenmMessage() {
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
    spdlog::debug("Created DENM message with following details:");
    spdlog::debug("Management Container:");
    spdlog::debug("  Action ID: {}", denm.getActionId());
    spdlog::debug("  Detection Time: {}", denm.getDetectionTimeFormatted());
    spdlog::debug("  Reference Time: {}", denm.getReferenceTimeFormatted());
    spdlog::debug("  Validity Duration: {}s", denm.getValidityDuration().count());
    
    spdlog::debug("Situation Container:");
    spdlog::debug("  Cause Code: {}", denm.getCauseCode());
    spdlog::debug("  Sub Cause Code: {}", denm.getSubCauseCode());
    spdlog::debug("  Information Quality: {}", denm.getInformationQuality());
    spdlog::debug("  Event Speed: {:.2f} m/s", denm.getEventSpeed());
    spdlog::debug("  Event Heading: {:.2f} degrees", denm.getEventHeading());

    spdlog::debug("Location Container:");
    spdlog::debug("  Position Confidence: {:.2f}", denm.getEventPositionConfidence());
    spdlog::debug("  Heading Confidence: {:.2f}", denm.getEventHeadingConfidence());
    spdlog::debug("  Speed Confidence: {:.2f}", denm.getEventSpeedConfidence());

    return denm;
}

int main(int argc, char **argv) {
    try {
        namespace po = boost::program_options;

        // Declare the supported options
        po::options_description desc("Allowed options");
        desc.add_options()
            ("help,h", "produce help message")
            ("cert-dir,c", po::value<std::string>()->default_value("ssl-certs/"), 
             "directory containing SSL certificates")
            ("log-level,l", po::value<std::string>()->default_value("info"),
             "logging level (debug, info, warn, error)")
        ;

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 0;
        }

        // Set log level
        std::string log_level = vm["log-level"].as<std::string>();
        if (log_level == "debug") {
            spdlog::set_level(spdlog::level::debug);
        } else if (log_level == "info") {
            spdlog::set_level(spdlog::level::info);
        } else if (log_level == "warn") {
            spdlog::set_level(spdlog::level::warn);
        } else if (log_level == "error") {
            spdlog::set_level(spdlog::level::err);
        } else {
            throw std::runtime_error("Invalid log level: " + log_level);
        }

        // Set certificate directory
        set_cert_directory(vm["cert-dir"].as<std::string>());
        
        // Create and configure DENM message
        DenmMessage denm = createDenmMessage();

        // Build the DENM message in a UPER encoded format
        auto denmMessage = denm.buildDenm();
        auto encodedDenm = denmMessage.encode();
        
        // Create a proton container
        proton::container container;
        
        // Create URL and address
        std::string url("amqp://localhost:5672");
        std::string address("examples");
        spdlog::info("Connecting to AMQP server at {}", url);

        // Start container thread
        auto container_thread = std::thread([&]() { container.run(); });

        // Create sender and receiver
        sender send(container, url, address, "sender");
        receiver recv(container, url, address, "receiver");
        
        // Small delay to ensure connections are established
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); 
        
        // Send the encoded DENM message
        proton::message amqp_msg;
        amqp_msg.body(proton::binary(encodedDenm.begin(), encodedDenm.end()));
        spdlog::info("Sending DENM message to: {}", address);
        send.send(amqp_msg);

        // Wait for response with timeout
        spdlog::info("Waiting for DENM message on: {}", address);
        auto start_time = std::chrono::steady_clock::now();
        bool received_response = false;
        while (!received_response) {
            try {
                auto received = recv.receive();
                if (received.body().empty()) {
                    spdlog::info("Received empty message");
                    continue;
                }
                
                // Decode received DENM message
                auto receivedData = proton::get<proton::binary>(received.body());
                DenmMessage receivedDenm;
                receivedDenm.fromUper(std::vector<unsigned char>(receivedData.data(), receivedData.data() + receivedData.size()));
                
                spdlog::info("Received and decoded DENM message:");
                spdlog::info("Management Container:");
                spdlog::info("  Action ID: {}", receivedDenm.getActionId());
                spdlog::info("  Detection Time: {}", receivedDenm.getDetectionTimeFormatted());
                spdlog::info("  Reference Time: {}", receivedDenm.getReferenceTimeFormatted());
                
                spdlog::info("Situation Container:");
                spdlog::info("  Cause Code: {}", receivedDenm.getCauseCode());
                spdlog::info("  Sub Cause Code: {}", receivedDenm.getSubCauseCode());
                spdlog::info("  Event Speed: {:.2f} m/s", receivedDenm.getEventSpeed());
                
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