#include "denm_service.hpp"
#include "geo_utils.hpp"
#include <spdlog/spdlog.h>

DenmService::DenmService(
    const std::string& amqp_url, 
    const std::string& amqp_send_address,
    const std::string& amqp_receive_address,
    const std::string& http_host,
    int http_port,
    int ws_port,
    bool receiver_mode,
    bool sender_mode
)
    : amqp_url_(amqp_url)
    , amqp_send_address_(amqp_send_address)
    , http_host_(http_host)
    , http_port_(http_port)
    , ws_port_(ws_port)
    , receiver_mode_(receiver_mode)
    , sender_mode_(sender_mode)
    , amqp_container_(std::make_unique<proton::container>())
    , running_(false) {
    
    setupRoutes();
}

DenmService::~DenmService() {
    stop();
}

void DenmService::setupRoutes() {
    // Setup Swagger documentation endpoint
    CROW_ROUTE(app_, "/api-docs")
    ([](const crow::request&) {
        crow::response res;
        res.code = 200;
        res.set_header("Content-Type", "text/html");
        res.body = R"(<!DOCTYPE html>
            <html>
            <head>
                <title>DENM Service API Documentation</title>
                <link rel="stylesheet" type="text/css" href="https://unpkg.com/swagger-ui-dist@4/swagger-ui.css">
            </head>
            <body>
                <div id="swagger-ui"></div>
                <script src="https://unpkg.com/swagger-ui-dist@4/swagger-ui-bundle.js"></script>
                <script>
                    window.onload = function() {
                        SwaggerUIBundle({
                            url: "/swagger.json",
                            dom_id: '#swagger-ui'
                        });
                    }
                </script>
            </body>
            </html>)";
        return res;
    });

    // Serve Swagger JSON specification
    CROW_ROUTE(app_, "/swagger.json")
    ([](const crow::request&) {
        crow::json::wvalue swagger;
        
        // Build the swagger object step by step
        swagger["openapi"] = "3.0.0";
        
        // Info object
        swagger["info"]["title"] = "DENM Service API Documentation";
        swagger["info"]["version"] = "1.0.0";
        swagger["info"]["description"] = "API for sending DENM messages via AMQP";
        
        // Paths object
        auto& denm_path = swagger["paths"]["/denm"]["post"];
        denm_path["summary"] = "Send a DENM message";
        
        // Request body
        auto& req_body = denm_path["requestBody"];
        req_body["required"] = true;
        auto& content = req_body["content"]["application/json"]["schema"];
        content["type"] = "object";
        content["required"] = crow::json::wvalue::list{"stationId", "latitude", "longitude", "publisherId", "originatingCountry"};
        
        // Properties
        auto& properties = content["properties"];
        properties["stationId"]["type"] = "integer";
        properties["actionId"]["type"] = "integer";
        properties["latitude"]["type"] = "number";
        properties["longitude"]["type"] = "number";
        properties["altitude"]["type"] = "number";
        properties["causeCode"]["type"] = "integer";
        properties["subCauseCode"]["type"] = "integer";
        properties["publisherId"]["type"] = "string";
        properties["originatingCountry"]["type"] = "string";
        
        // Responses
        auto& responses = denm_path["responses"];
        responses["200"]["description"] = "DENM message sent successfully";
        responses["200"]["content"]["application/json"]["schema"]["type"] = "object";
        responses["200"]["content"]["application/json"]["schema"]["properties"]["status"]["type"] = "string";
        
        responses["400"]["description"] = "Invalid request";
        responses["400"]["content"]["application/json"]["schema"]["type"] = "object";
        responses["400"]["content"]["application/json"]["schema"]["properties"]["error"]["type"] = "string";
        
        return crow::response(200, swagger);
    });

    // DENM message endpoint
    CROW_ROUTE(app_, "/denm")
        .methods("POST"_method)
        ([this](const crow::request& req) {
            crow::response res;
            this->handleDenmPost(req, res);
            return res;
        });
}

void DenmService::handleDenmPost(const crow::request& req, crow::response& res) {
    try {
        auto j = crow::json::load(req.body);
        if (!j) {
            res.code = 400;
            res.write("{\"error\":\"Invalid JSON\"}");
            return;
        }
        
        // Create DENM message from JSON
        auto denm = createDenmFromJson(j);
        
        // Create AMQP message
        proton::message amqp_msg;
        
        // Build and encode DENM
        auto denmMessage = denm.buildDenm();
        auto encodedDenm = denmMessage.encode();
        
        // Set message body
        amqp_msg.body(proton::binary(encodedDenm.begin(), encodedDenm.end()));
        
        // Set AMQP headers
        amqp_msg.durable(true);
        amqp_msg.ttl(proton::duration(3600000)); // 1 hour TTL
        amqp_msg.priority(1);
        
        // Set application properties
        amqp_msg.properties().put("publisherId", j["publisherId"].s());
        amqp_msg.properties().put("publicationId", std::string(j["publisherId"].s()) + ":DENM_001");
        amqp_msg.properties().put("originatingCountry", j["originatingCountry"].s());
        amqp_msg.properties().put("protocolVersion", "DENM:1.2.2");
        amqp_msg.properties().put("messageType", "DENM");
        amqp_msg.properties().put("latitude", j["latitude"].d());
        amqp_msg.properties().put("longitude", j["longitude"].d());
        
        // Calculate and set quadTree
        std::string quadTree = calculateQuadTree(j["latitude"].d(), j["longitude"].d());
        amqp_msg.properties().put("quadTree", "," + quadTree + ",");
        
        // Send message using our sender class
        amqp_sender_->send(amqp_msg);
        
        res.code = 200;
        res.write("{\"status\":\"success\"}");
        
    } catch (const std::exception& e) {
        spdlog::error("Error processing DENM request: {}", e.what());
        res.code = 400;
        res.write("{\"error\":\"" + std::string(e.what()) + "\"}");
    }
}

DenmMessage DenmService::createDenmFromJson(const crow::json::rvalue& j) {
    DenmMessage denm;
    
    denm.setStationId(j["stationId"].i());
    denm.setActionId(j.has("actionId") ? j["actionId"].i() : 1);
    
    time_t timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    denm.setDetectionTime(timestamp);
    denm.setReferenceTime(timestamp);
    
    denm.setEventPosition(
        j["latitude"].d(),
        j["longitude"].d(),
        j.has("altitude") ? j["altitude"].d() : 0.0
    );
    
    denm.setRelevanceDistance(RelevanceDistance_lessThan50m);
    denm.setRelevanceTrafficDirection(RelevanceTrafficDirection_allTrafficDirections);
    denm.setValidityDuration(std::chrono::seconds(600));
    denm.setStationType(j.has("stationType") ? j["stationType"].i() : 3);
    
    denm.setInformationQuality(j.has("informationQuality") ? j["informationQuality"].i() : 0);
    denm.setCauseCode(static_cast<CauseCodeType_t>(j.has("causeCode") ? j["causeCode"].i() : 1));
    denm.setSubCauseCode(j.has("subCauseCode") ? j["subCauseCode"].i() : 0);
    
    return denm;
}

void DenmService::start() {
    if (running_) return;
    
    running_ = true;
    
    // Create AMQP sender first
    try {
        spdlog::debug("Creating AMQP sender with URL: {} and address: {}", amqp_url_, amqp_send_address_);
        amqp_sender_ = std::make_unique<sender>(*amqp_container_, amqp_url_, amqp_send_address_, "denm-sender");
    } catch (const std::exception& e) {
        spdlog::error("Failed to create AMQP sender: {}", e.what());
        running_ = false;
        throw;
    }
    
    // Start AMQP container in a separate thread
    container_thread_ = std::thread([this]() {
        try {
            spdlog::debug("Starting AMQP container");
            amqp_container_->run();
        } catch (const std::exception& e) {
            spdlog::error("AMQP container error: {}", e.what());
        }
    });
    
    // Configure HTTP server
    app_.loglevel(crow::LogLevel::Warning);  // Reduce noise from Crow
    
    // Start HTTP server (this will block)
    spdlog::info("Starting HTTP server on {}:{}", http_host_, http_port_);
    try {
        // Use "0.0.0.0" to bind to all available network interfaces
        app_.bindaddr("0.0.0.0").port(http_port_).run();
    } catch (const std::exception& e) {
        spdlog::error("HTTP server error: {}", e.what());
        stop();  // Clean up AMQP resources
        throw;
    }
}

void DenmService::stop() {
    if (!running_) return;
    
    running_ = false;
    
    // Stop HTTP server
    app_.stop();
    
    // Close AMQP sender and stop container
    if (amqp_sender_) {
        amqp_sender_->close();
    }
    
    if (container_thread_.joinable()) {
        container_thread_.join();
    }
}