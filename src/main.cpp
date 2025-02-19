#include "denm_service.hpp"
#include "ssl_utils.hpp"
#include <boost/program_options.hpp>
#include <spdlog/spdlog.h>
#include <csignal>
#include <condition_variable>
#include <mutex>

std::unique_ptr<DenmService> service;

void signal_handler(int signal) {
    spdlog::info("Received signal {}, shutting down...", signal);
    if (service) {
        service->stop();
    }
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
            ("amqp-url", po::value<std::string>()->default_value("amqp://localhost:5672"),
             "AMQP broker URL")
            ("amqp-send", po::value<std::string>()->default_value("examples"),
             "AMQP send address")
            ("amqp-receive", po::value<std::string>()->default_value("examples"),
             "AMQP receive address")
            ("http-host", po::value<std::string>()->default_value("localhost"),
             "HTTP server host")
            ("http-port", po::value<int>()->default_value(8080),
             "HTTP server port")
            ("ws-port", po::value<int>()->default_value(8081),
             "WebSocket server port")
            ("receiver", po::value<bool>()->default_value(true),
             "Enable receiver mode")
            ("sender", po::value<bool>()->default_value(true),
             "Enable sender mode")
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

        // Setup signal handling
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);

        // Create and start service
        service = std::make_unique<DenmService>(
            vm["amqp-url"].as<std::string>(),
            vm["amqp-send"].as<std::string>(),
            vm["amqp-receive"].as<std::string>(),
            vm["http-host"].as<std::string>(),
            vm["http-port"].as<int>(),
            vm["ws-port"].as<int>(),
            vm["receiver"].as<bool>(),
            vm["sender"].as<bool>()
        );

        spdlog::info("Starting DENM service...");
        service->start();

        // Keep main thread alive until signal is received
        std::condition_variable cv;
        std::mutex mtx;
        std::unique_lock<std::mutex> lock(mtx);
        
        auto old_handler = signal(SIGINT, [](int) {
            if (service) {
                service->stop();
            }
            exit(0);  // Force exit if service->stop() doesn't complete
        });

        // Wait indefinitely
        cv.wait(lock);

        return 0;
    } catch (const std::exception& e) {
        spdlog::error("Error: {}", e.what());
        return 1;
    }
}