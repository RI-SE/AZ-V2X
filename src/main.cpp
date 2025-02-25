#include "denm_service.hpp"
#include "ssl_utils.hpp"
#include <boost/program_options.hpp>
#include <condition_variable>
#include <csignal>
#include <mutex>
#include <spdlog/spdlog.h>
#include "interchange_service.hpp"

std::unique_ptr<DenmService> service;

void signal_handler(int signal) {
	spdlog::info("Received signal {}, shutting down...", signal);
	if (service) {
		service->stop();
	}
}

int main(int argc, char** argv) {
	try {
		namespace po = boost::program_options;

		// Declare the supported options
		po::options_description desc("Allowed options");
		desc.add_options()
			("help,h", "produce help message")
			("cert-dir,c", 
			 po::value<std::string>()->default_value(
				 getenv("CERT_DIR") ? getenv("CERT_DIR") : ""), 
			 "directory containing SSL certificates")
			("log-level,l", 
			 po::value<std::string>()->default_value(
				 getenv("LOG_LEVEL") ? getenv("LOG_LEVEL") : "info"), 
			 "logging level (debug, info, warn, error)")
			("amqp-url", 
			 po::value<std::string>()->default_value(
				 getenv("AMQP_URL") ? getenv("AMQP_URL") : "amqp://localhost:5672"), 
			 "AMQP broker URL")
			("amqp-send", 
			 po::value<std::string>()->default_value(
				 getenv("AMQP_SEND") ? getenv("AMQP_SEND") : "examples"), 
			 "AMQP send address")
			("amqp-receive", 
			 po::value<std::string>()->default_value(
				 getenv("AMQP_RECEIVE") ? getenv("AMQP_RECEIVE") : "examples"), 
			 "AMQP receive address")
			("http-host", 
			 po::value<std::string>()->default_value(
				 getenv("HTTP_HOST") ? getenv("HTTP_HOST") : "0.0.0.0"), 
			 "HTTP server host")
			("http-port", 
			 po::value<int>()->default_value(
				 getenv("HTTP_PORT") ? std::stoi(getenv("HTTP_PORT")) : 8080), 
			 "HTTP server port")
			("ws-port", 
			 po::value<int>()->default_value(
				 getenv("WS_PORT") ? std::stoi(getenv("WS_PORT")) : 8081), 
			 "WebSocket server port")
			("receiver", 
			 po::value<bool>()->default_value(
				 getenv("RECEIVER") ? std::string(getenv("RECEIVER")) == "true" : true), 
			 "Enable receiver mode")
			("sender", 
			 po::value<bool>()->default_value(
				 getenv("SENDER") ? std::string(getenv("SENDER")) == "true" : true), 
			 "Enable sender mode");

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

		// Setup signal handling
		std::signal(SIGINT, [](int) {
			spdlog::info("Received SIGINT, shutting down...");
			if (service) {
				service->stop();
			}
			exit(0);  // Ensure we exit if stop() hangs
		});

		// Create services
		auto interchange = std::make_unique<InterchangeService>(
			vm["amqp-url"].as<std::string>(),
			vm["amqp-send"].as<std::string>(),
			vm["amqp-receive"].as<std::string>(),
			vm["cert-dir"].as<std::string>()
		);

		service = std::make_unique<DenmService>(
			vm["http-host"].as<std::string>(),
			vm["http-port"].as<int>(),
			vm["ws-port"].as<int>()
		);

		// Start services
		interchange->start();
		service->start();

		// Keep main thread alive with a simple pause
		pause();  // This will block until a signal is received

		return 0;
	} catch (const std::exception& e) {
		spdlog::error("Error: {}", e.what());
		return 1;
	}
}
