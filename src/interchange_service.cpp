#include "interchange_service.hpp"
#include "denm_message.hpp"
#include "geo_utils.hpp"
#include <proton/connection_options.hpp>
#include <proton/reconnect_options.hpp>
#include <spdlog/spdlog.h>

InterchangeService::InterchangeService(const std::string& username,
									   const std::string& amqp_url,
									   const std::string& amqp_send_address,
									   const std::string& amqp_receive_address,
									   const std::string& cert_dir) :
  username_(username),
  amqp_url_(amqp_url),
  amqp_send_address_(amqp_send_address),
  amqp_receive_address_(amqp_receive_address),
  cert_dir_(cert_dir),
  amqp_container_(std::make_unique<proton::container>()) {

	// Configure container settings
	setupContainerOptions();

	// Subscribe to outgoing DENM events
	EventBus::getInstance().subscribe("denm.outgoing",
									  [this](const nlohmann::json& denm) { this->handleOutgoingDenm(denm); });
}

void InterchangeService::setupContainerOptions() {
	proton::connection_options conn_opts;

	if (!cert_dir_.empty()) {
		try {
			ssl_certificate client_cert = platform_certificate(username_, cert_dir_);
			std::string server_CA		= platform_CA("truststore");

			proton::ssl_client_options ssl_cli(client_cert, server_CA, proton::ssl::VERIFY_PEER);
			conn_opts.ssl_client_options(ssl_cli);
			spdlog::info("SSL enabled for AMQP connection");
		} catch (const std::exception& e) {
			spdlog::error("Failed to configure SSL: {}", e.what());
			throw;
		}
	}

	// Match Java client configuration
	conn_opts.user(username_)
	  .sasl_enabled(true)
	  .sasl_allowed_mechs("EXTERNAL PLAIN")
	  .container_id(username_ + "-az-client");

	// Add retry mechanism
	conn_opts.reconnect(proton::reconnect_options()
						  .delay(proton::duration(1000))	  // 1 second initial delay
						  .max_delay(proton::duration(10000)) // 10 seconds max delay
						  .max_attempts(5));				  // Maximum 5 retry attempts

	amqp_container_->client_connection_options(conn_opts);
}

void InterchangeService::start() {
	if (running_)
		return;
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
	if (!amqp_send_address_.empty()) {
		setupAmqpSender();
	}
	// Setup receiver
	if (!amqp_receive_address_.empty()) {
		setupAmqpReceiver();
	}
}

void InterchangeService::setupAmqpSender() {
	// Setup sender with retry
	int retry_count		  = 0;
	const int max_retries = 5;
	while (retry_count < max_retries) {
		try {
			amqp_sender_ =
			  std::make_unique<sender>(*amqp_container_, amqp_url_, amqp_send_address_, username_ + "-az-sender");
			break;
		} catch (const std::exception& e) {
			spdlog::warn("Failed to create sender (attempt {}/{}): {}", retry_count + 1, max_retries, e.what());
			std::this_thread::sleep_for(std::chrono::seconds(3));
			retry_count++;
		}
	}

	if (!amqp_sender_) {
		throw std::runtime_error("Failed to create sender after max retries");
	}
}

void InterchangeService::setupAmqpReceiver() {
	amqp_receiver_ =
	  std::make_unique<receiver>(*amqp_container_, amqp_url_, amqp_receive_address_, username_ + "-az-receiver");

	receiver_thread_ = std::thread([this]() {
		while (running_) {
			try {
				proton::message msg = amqp_receiver_->receive();
				spdlog::debug("Received DENM message");
				if (msg.body().type() == proton::BINARY) {
					auto data = proton::get<proton::binary>(msg.body());
					DenmMessage denm;
					denm.fromUper(data);
					// Publish received DENM to event bus
					EventBus::getInstance().publish("denm.incoming", denm.toJson());
				} else {
					spdlog::error("Received non-binary message");
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

void InterchangeService::handleOutgoingDenm(const nlohmann::json& j) {
	try {

		proton::message amqp_msg;
		// Set AMQP headers
		amqp_msg.durable(true);
		amqp_msg.ttl(proton::duration(3600000)); // 1 hour TTL
		amqp_msg.priority(1);
		amqp_msg.user(username_);
		amqp_msg.to(amqp_send_address_);

		auto& props = amqp_msg.properties();

		// Set mandatory properties
		props.put("messageType", j["messageType"].get<std::string>());
		props.put("protocolVersion", j["protocolVersion"].get<std::string>());
		props.put("publisherId", j["publisherId"].get<std::string>());
		props.put("publicationId", j["publicationId"].get<std::string>());
		props.put("originatingCountry", j["originatingCountry"].get<std::string>());
		props.put("causeCode", j["data"]["situation"]["causeCode"].get<int>());

		// Calculate quadTree unless it is already present
		if (j.contains("quadTree")) {
			props.put("quadTree", j["quadTree"].get<std::string>());

		} else {
			std::string quadTree = calculateQuadTree(j["latitude"].get<double>(), j["longitude"].get<double>());
			spdlog::debug("Calculated quad tree: {}", quadTree);
			auto formattedQuadTree = "," + quadTree + ",";
			props.put("quadTree", formattedQuadTree);
		}

		// Set optional properties if present
		if (j.contains("shardId")) {
			props.put("shardId", j["shardId"].get<int>());
		}
		if (j.contains("shardCount")) {
			props.put("shardCount", j["shardCount"].get<int>());
		}
		if (j.contains("timestamp")) {
			props.put("timestamp", j["timestamp"].get<std::string>());
		}
		if (j.contains("relation")) {
			props.put("relation", j["relation"].get<std::string>());
		}

		// Create binary message
		DenmMessage denm = DenmMessage::fromJson(j["data"]);
		auto raw_body	 = denm.getUperEncoded();

		// Convert std::vector<unsigned char> to proton::binary
		proton::binary body(raw_body.begin(), raw_body.end());
		amqp_msg.body(body);
		amqp_sender_->send(amqp_msg);

		spdlog::debug("Successfully sent DENM message");

	} catch (const nlohmann::json::exception& e) {
		spdlog::error("JSON error while processing DENM: {}", e.what());
		throw;
	} catch (const std::exception& e) {
		spdlog::error("Failed to send DENM: {}", e.what());
		throw;
	}
}

void InterchangeService::stop() {
	if (!running_)
		return;
	running_ = false;

	amqp_container_->stop();

	if (amqp_sender_)
		amqp_sender_->close();
	if (amqp_receiver_)
		amqp_receiver_->close();

	if (receiver_thread_.joinable())
		receiver_thread_.join();
	if (container_thread_.joinable())
		container_thread_.join();
}
