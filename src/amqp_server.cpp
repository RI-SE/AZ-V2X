#include "amqp_server.hpp"
#include "ssl_utils.hpp"
#include <proton/connection_options.hpp>
#include <proton/container.hpp>
#include <proton/message.hpp>
#include <proton/ssl.hpp>
#include <proton/transport.hpp>
#include <proton/target.hpp>
#include <proton/delivery.hpp>
#include <proton/link.hpp>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <mutex>
#include <condition_variable>

void amqp_server::listener_open_handler::on_open(proton::listener& l) {
    std::cout << "Server listening on " << l.port() << std::endl;
    std::lock_guard<std::mutex> lock(mutex_);
    ready_ = true;
    cv_.notify_all();
}

amqp_server::amqp_server(proton::container& cont, const std::string& url, const std::string& address)
    : is_ready_(false), 
      listen_handler(server_ready_, is_ready_, lock_)
{
    // Initialize SSL settings and start listening
    ssl_certificate server_cert = platform_certificate("server", "");
    std::string client_CA = platform_CA("ca");
    proton::ssl_server_options srv_ssl(server_cert, client_CA);
    proton::connection_options server_opts;
    server_opts.ssl_server_options(srv_ssl).handler(s_handler);
    server_opts.sasl_allowed_mechs("EXTERNAL");
    cont.server_connection_options(server_opts);

    // Start listening
    s_handler.listener = cont.listen(url, listen_handler);
}

void amqp_server::wait_for_ready() {
    std::unique_lock<std::mutex> lock(lock_);
    server_ready_.wait(lock, [this]() { return is_ready_; });
}

void amqp_server::on_container_start(proton::container& c) {
    // Container is already started and listening from constructor
}

void amqp_server::on_tracker_accept(proton::tracker& t) {
    // Implementation depends on your needs
    // This is a basic empty implementation
}