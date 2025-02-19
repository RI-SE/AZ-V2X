#include "denm_service.hpp"
#include "geo_utils.hpp"
#include <spdlog/spdlog.h>

DenmService::DenmService(const std::string& amqp_url, const std::string& amqp_address,
                       const std::string& http_host, int http_port)
    : amqp_url_(amqp_url)
    , amqp_address_(amqp_address)
    , http_host_(http_host)
    , http_port_(http_port)
    , server_(std::make_unique<httplib::Server>())
    , container_(std::make_unique<proton::container>())
    , running_(false) {
    
    setupRoutes();
}

DenmService::~DenmService() {
    stop();
}

void DenmService::setupRoutes() {
    // Setup Swagger documentation endpoint
    server_->Get("/api-docs", [](const httplib::Request&, httplib::Response& res) {
        // Serve Swagger UI
        res.set_content(R"(
            <!DOCTYPE html>
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
            </html>
        )", "text/html");
    });

    // Serve Swagger JSON specification
    server_->Get("/swagger.json", [](const httplib::Request&, httplib::Response& res) {
        nlohmann::json swagger = {
            {"openapi", "3.0.0"},
            {"info", {
                {"title", "DENM Message Service API"},
                {"version", "1.0.0"},
                {"description", "API for sending DENM messages via AMQP"}
            }},
            {"paths", {
                {"/denm", {
                    {"post", {
                        {"summary", "Send a DENM message"},
                        {"requestBody", {
                            {"required", true},
                            {"content", {
                                {"application/json", {
                                    {"schema", {
                                        {"type", "object"},
                                        {"properties", {
                                            {"stationId", {{"type", "integer"}}},
                                            {"actionId", {{"type", "integer"}}},
                                            {"latitude", {{"type", "number"}}},
                                            {"longitude", {{"type", "number"}}},
                                            {"altitude", {{"type", "number"}}},
                                            {"causeCode", {{"type", "integer"}}},
                                            {"subCauseCode", {{"type", "integer"}}},
                                            {"publisherId", {{"type", "string"}}},
                                            {"originatingCountry", {{"type", "string"}}}
                                        }},
                                        {"required", {"stationId", "latitude", "longitude", "publisherId", "originatingCountry"}}
                                    }}
                                }}
                            }}
                        }},
                        {"responses", {
                            {"200", {
                                {"description", "DENM message sent successfully"}
                            }},
                            {"400", {
                                {"description", "Invalid request"}
                            }},
                            {"500", {
                                {"description", "Internal server error"}
                            }}
                        }}
                    }}
                }}
            }}
        };
        
        res.set_content(swagger.dump(2), "application/json");
    });

    // DENM message endpoint
    server_->Post("/denm", [this](const httplib::Request& req, httplib::Response& res) {
        this->handleDenmPost(req, res);
    });
}

void DenmService::handleDenmPost(const httplib::Request& req, httplib::Response& res) {
    try {
        auto j = nlohmann::json::parse(req.body);
        
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
        amqp_msg.properties().put("publisherId", j["publisherId"].get<std::string>());
        amqp_msg.properties().put("publicationId", j["publisherId"].get<std::string>() + ":DENM_001");
        amqp_msg.properties().put("originatingCountry", j["originatingCountry"].get<std::string>());
        amqp_msg.properties().put("protocolVersion", "DENM:1.2.2");
        amqp_msg.properties().put("messageType", "DENM");
        amqp_msg.properties().put("latitude", j["latitude"].get<double>());
        amqp_msg.properties().put("longitude", j["longitude"].get<double>());
        
        // Calculate and set quadTree
        std::string quadTree = calculateQuadTree(j["latitude"].get<double>(), j["longitude"].get<double>());
        amqp_msg.properties().put("quadTree", "," + quadTree + ",");
        
        // Send message
        amqp_sender_->send(amqp_msg);
        
        res.status = 200;
        res.set_content("{\"status\":\"success\"}", "application/json");
        
    } catch (const std::exception& e) {
        spdlog::error("Error processing DENM request: {}", e.what());
        res.status = 400;
        res.set_content("{\"error\":\"" + std::string(e.what()) + "\"}", "application/json");
    }
}

DenmMessage DenmService::createDenmFromJson(const nlohmann::json& j) {
    DenmMessage denm;
    
    denm.setStationId(j["stationId"].get<long>());
    denm.setActionId(j.value("actionId", 1));
    
    time_t timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    denm.setDetectionTime(timestamp);
    denm.setReferenceTime(timestamp);
    
    denm.setEventPosition(
        j["latitude"].get<double>(),
        j["longitude"].get<double>(),
        j.value("altitude", 0.0)
    );
    
    denm.setRelevanceDistance(RelevanceDistance_lessThan50m);
    denm.setRelevanceTrafficDirection(RelevanceTrafficDirection_allTrafficDirections);
    denm.setValidityDuration(std::chrono::seconds(600));
    denm.setStationType(j.value("stationType", 3));
    
    denm.setInformationQuality(j.value("informationQuality", 0));
    denm.setCauseCode(static_cast<CauseCodeType_t>(j.value("causeCode", 1)));
    denm.setSubCauseCode(j.value("subCauseCode", 0));
    
    return denm;
}

void DenmService::start() {
    if (running_) return;
    
    running_ = true;
    
    // Start AMQP container
    container_thread_ = std::thread([this]() {
        container_->run();
    });
    
    // Create AMQP sender
    amqp_sender_ = std::make_unique<sender>(*container_, amqp_url_, amqp_address_, "sender");
    
    // Start HTTP server
    spdlog::info("Starting HTTP server on {}:{}", http_host_, http_port_);
    server_->listen(http_host_, http_port_);
}

void DenmService::stop() {
    if (!running_) return;
    
    running_ = false;
    
    // Stop HTTP server
    server_->stop();
    
    // Close AMQP sender and stop container
    if (amqp_sender_) {
        amqp_sender_->close();
    }
    
    if (container_thread_.joinable()) {
        container_thread_.join();
    }
} 