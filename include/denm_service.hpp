#ifndef DENM_SERVICE_HPP
#define DENM_SERVICE_HPP

#include "amqp_client.hpp"
#include "denm_message.hpp"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <string>
#include <memory>
#include <proton/container.hpp>

class DenmService {
public:
    DenmService(const std::string& amqp_url, const std::string& amqp_address, 
                const std::string& http_host = "localhost", int http_port = 8080);
    ~DenmService();

    void start();
    void stop();

private:
    void setupRoutes();
    void handleDenmPost(const httplib::Request& req, httplib::Response& res);
    DenmMessage createDenmFromJson(const nlohmann::json& j);
    
    std::string amqp_url_;
    std::string amqp_address_;
    std::string http_host_;
    int http_port_;
    
    std::unique_ptr<httplib::Server> server_;
    std::unique_ptr<proton::container> container_;
    std::unique_ptr<sender> amqp_sender_;
    std::thread container_thread_;
    bool running_;
};

#endif // DENM_SERVICE_HPP 