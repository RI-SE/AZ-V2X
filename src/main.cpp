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
#include "geo_utils.hpp"

DenmMessage createDenmMessage() {
    DenmMessage denm;
    
    // Set header values
    denm.setStationId(1234567);

    denm.setActionId(20);
    
    time_t timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    denm.setDetectionTime(timestamp);
    denm.setReferenceTime(timestamp);
    
    // Set event position (convert from microdegrees to degrees)
    double latitude = 57.779017;  
    double longitude = 12.774981;
    double altitude = 190.0;
    denm.setEventPosition(latitude, longitude, altitude);
    
    // Set awareness (relevance) distance
    denm.setRelevanceDistance(RelevanceDistance_lessThan50m);
    
    // Set awareness (relevance) traffic direction
    denm.setRelevanceTrafficDirection(RelevanceTrafficDirection_allTrafficDirections);
    
    // Set validity duration (600 seconds)
    denm.setValidityDuration(std::chrono::seconds(600));
    
    // Set station type (3 for moped/e-scooter)
    denm.setStationType(3);
    
    // Set situation container values
    denm.setInformationQuality(0);  // Unavailable
    denm.setCauseCode(CauseCodeType_accident);  // 1 for accident
    denm.setSubCauseCode(0);  // Not specified
    
    // Log DENM message details
    spdlog::debug("Created DENM message with following details:");
    spdlog::debug("Management Container:");
    spdlog::debug("  Action ID: {}", denm.getActionId());
    spdlog::debug("  Protocol Version: {}", denm.getProtocolVersion());
    spdlog::debug("  Detection Time: {}", denm.getDetectionTimeFormatted());
    spdlog::debug("  Reference Time: {}", denm.getReferenceTimeFormatted());
    spdlog::debug("  Validity Duration: {}s", denm.getValidityDuration().count());
    
    spdlog::debug("Situation Container:");
    spdlog::debug("  Cause Code: {}", denm.getCauseCode());
    spdlog::debug("  Sub Cause Code: {}", denm.getSubCauseCode());
    spdlog::debug("  Information Quality: {}", denm.getInformationQuality());

    spdlog::debug("Location Container:");
    spdlog::debug("  Event Position: {} {} {}", denm.getEventPosition().latitude, denm.getEventPosition().longitude, denm.getEventPosition().altitude.altitudeValue);
    

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

        // First encode the DENM message
        DenmMessage denm = createDenmMessage();

        // Build the DENM message in a UPER encoded format
        auto denmMessage = denm.buildDenm();
        auto encodedDenm = denmMessage.encode();

        // Set message body
        amqp_msg.body(proton::binary(encodedDenm.begin(), encodedDenm.end()));

        // Set AMQP headers
        amqp_msg.durable(true);
        amqp_msg.ttl(proton::duration(3600000)); // 1 hour TTL
        amqp_msg.priority(1);

        // Set application-specific properties using message.properties()
        amqp_msg.properties().put("publisherId", "NO00001");  // Replace with your publisher ID
        amqp_msg.properties().put("publicationId", "NO00001:DENM_001");  // Replace with your publication ID
        amqp_msg.properties().put("originatingCountry", "NO");  // Replace with your country code
        amqp_msg.properties().put("protocolVersion", "DENM:1.2.2");
        amqp_msg.properties().put("messageType", "DENM");

        // Get position from DENM message for latitude/longitude
        const auto& pos = denm.getEventPosition();
        double lat = pos.latitude / 10000000.0;  // Convert from 1/10 microdegrees to degrees
        double lon = pos.longitude / 10000000.0;

        amqp_msg.properties().put("latitude", lat);
        amqp_msg.properties().put("longitude", lon);

        // Calculate quadTree value based on lat/lon
        std::string quadTree = calculateQuadTree(lat, lon);
        amqp_msg.properties().put("quadTree", "," + quadTree + ",");

        // Optional properties
        amqp_msg.properties().put("timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

        // If using sharding (optional)
        // props.set_property("shardId", 1);
        // props.set_property("shardCount", 1);

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