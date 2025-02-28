#include "denm_service.hpp"
#include "event_bus.hpp"
#include "geo_utils.hpp"
#include <spdlog/spdlog.h>

DenmService::DenmService(const std::string& http_host, int http_port, int ws_port) :
  http_host_(http_host),
  http_port_(http_port),
  ws_port_(ws_port),
  running_(false) {
	// Setup HTTP routes (including WebSocket)
	setupRoutes();
	EventBus::getInstance().subscribe("denm.incoming",
									  [this](const nlohmann::json& denm) { this->broadcastMessage(denm.dump()); });
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
		swagger["info"]["title"]	   = "DENM Service API Documentation";
		swagger["info"]["version"]	   = "1.0.0";
		swagger["info"]["description"] = "API for sending DENM messages via AMQP";

		// Paths object
		auto& denm_path		 = swagger["paths"]["/denm"]["post"];
		denm_path["summary"] = "Send a DENM message";

		// Request body
		auto& req_body		 = denm_path["requestBody"];
		req_body["required"] = true;
		auto& content		 = req_body["content"]["application/json"]["schema"];
		content["type"]		 = "object";
		content["required"] =
		  crow::json::wvalue::list{"stationId", "latitude", "longitude", "publisherId", "originatingCountry"};

		// Properties
		auto& properties						 = content["properties"];
		properties["stationId"]["type"]			 = "integer";
		properties["actionId"]["type"]			 = "integer";
		properties["latitude"]["type"]			 = "number";
		properties["longitude"]["type"]			 = "number";
		properties["altitude"]["type"]			 = "number";
		properties["causeCode"]["type"]			 = "integer";
		properties["subCauseCode"]["type"]		 = "integer";
		properties["publisherId"]["type"]		 = "string";
		properties["originatingCountry"]["type"] = "string";

		// Responses
		auto& responses													  = denm_path["responses"];
		responses["200"]["description"]									  = "DENM message sent successfully";
		responses["200"]["content"]["application/json"]["schema"]["type"] = "object";
		responses["200"]["content"]["application/json"]["schema"]["properties"]["status"]["type"] = "string";

		responses["400"]["description"]															 = "Invalid request";
		responses["400"]["content"]["application/json"]["schema"]["type"]						 = "object";
		responses["400"]["content"]["application/json"]["schema"]["properties"]["error"]["type"] = "string";

		return crow::response(200, swagger);
	});

	// DENM message endpoint
	CROW_ROUTE(app_, "/denm").methods("POST"_method)([this](const crow::request& req) {
		crow::response res;
		this->handleDenmPost(req, res);
		return res;
	});

	// New WebSocket endpoint for relaying AMQP messages to the Vue.js client
	CROW_ROUTE(app_, "/denm")
	  .websocket()
	  .onopen([this](crow::websocket::connection& conn) {
		  {
			  std::lock_guard<std::mutex> lock(ws_connections_mutex_);
			  ws_connections_.insert(&conn);
		  }
		  spdlog::info("WebSocket connection opened.");
	  })
	  .onclose([this](crow::websocket::connection& conn, const std::string& reason) {
		  {
			  std::lock_guard<std::mutex> lock(ws_connections_mutex_);
			  ws_connections_.erase(&conn);
		  }
		  spdlog::info("WebSocket connection closed: {}", reason);
	  })
	  .onmessage([](crow::websocket::connection& conn, const std::string& data, bool is_binary) {
		  // Optionally react to messages from the frontend
		  spdlog::debug("Received WS message: {}", data);
	  });
}

void DenmService::handleDenmPost(const crow::request& req, crow::response& res) {
	try {
		// Parse the JSON request
		auto j = crow::json::load(req.body);
		if (!j) {
			res.code = 400;
			res.write("{\"error\":\"Invalid JSON\"}");
			return;
		}

		// Convert crow::json to nlohmann::json for EventBus
		nlohmann::json denm_json = nlohmann::json::parse(req.body);

		// Debug log the parsed JSON
		spdlog::debug("Parsed DENM JSON: {}", denm_json.dump(2));

		// Publish the DENM message to the event bus
		EventBus::getInstance().publish("denm.outgoing", denm_json);

		res.code = 200;
		res.write("{\"status\":\"success\"}");
	} catch (const std::exception& e) {
		spdlog::error("Error processing DENM request: {}", e.what());
		res.code = 400;
		res.write("{\"error\":\"" + std::string(e.what()) + "\"}");
	}
}

void DenmService::start() {
	if (running_)
		return;

	running_ = true;

	// Start HTTP server (with WebSocket support) in a separate thread
	http_thread_ = std::thread([this]() {
		spdlog::info("Starting HTTP server on {}:{}", http_host_, http_port_);
		app_.bindaddr("0.0.0.0").port(http_port_).run();
	});
}

void DenmService::stop() {
	if (!running_)
		return;

	running_ = false;

	// Stop the HTTP server (which stops WebSocket and REST endpoints)
	app_.stop();

	if (http_thread_.joinable()) {
		http_thread_.join();
	}
}

// New helper method to broadcast a message to all connected WebSocket clients.
void DenmService::broadcastMessage(const std::string& message) {
	spdlog::debug("Broadcasting message to WebSocket clients: {}", message);
	std::lock_guard<std::mutex> lock(ws_connections_mutex_);
	for (auto* conn : ws_connections_) {
		conn->send_text(message);
	}
}
