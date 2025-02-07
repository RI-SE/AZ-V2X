#ifndef AMQP_SERVER_HPP
#define AMQP_SERVER_HPP

#include <proton/messaging_handler.hpp>
#include <proton/listen_handler.hpp>
#include <proton/listener.hpp>
#include "server_handler.hpp"
#include <condition_variable>
#include <mutex>

class amqp_server : public proton::messaging_handler {
private:
    class listener_open_handler : public proton::listen_handler {
    public:
        listener_open_handler(std::condition_variable& cv, bool& ready, std::mutex& mutex) 
            : cv_(cv), ready_(ready), mutex_(mutex) {}
        
        void on_open(proton::listener& l) override;
    private:
        std::condition_variable& cv_;
        bool& ready_;
        std::mutex& mutex_;
    };

    std::mutex lock_;
    std::condition_variable server_ready_;
    bool is_ready_;
    listener_open_handler listen_handler;
    server_handler s_handler;
    proton::listener listener_;

public:
    amqp_server(proton::container& cont, const std::string& url, const std::string& address);
    void on_container_start(proton::container &c) override;
    void on_tracker_accept(proton::tracker &t) override;
    
    // Wait for server to be ready
    void wait_for_ready();
};

#endif // AMQP_SERVER_HPP 