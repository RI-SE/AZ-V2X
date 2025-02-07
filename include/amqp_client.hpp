#ifndef AMQP_CLIENT_HPP
#define AMQP_CLIENT_HPP

#include <proton/connection.hpp>
#include <proton/container.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/message.hpp>
#include <string>
#include <mutex>
#include <condition_variable>
#include <queue>

// Exception raised if a sender or receiver is closed when trying to send/receive
class closed : public std::runtime_error {
  public:
    closed(const std::string& msg) : std::runtime_error(msg) {}
};

// A thread-safe sending connection
class sender : private proton::messaging_handler {
public:
    sender(proton::container& cont, const std::string& url, const std::string& address);
    void send(const proton::message& m);
    void close();
    std::string reply_address() const { return address_ + "-reply"; }

private:
    proton::sender sender_;
    std::mutex lock_;
    proton::work_queue* work_queue_;
    std::condition_variable sender_ready_;
    int queued_;
    int credit_;
    std::string address_;

    // Handler methods
    void on_connection_open(proton::connection& cont) override;
    void on_sender_open(proton::sender& s) override;
    void on_sendable(proton::sender& s) override;
    void on_error(const proton::error_condition& e) override;
    
    proton::work_queue* work_queue();
    void do_send(const proton::message& m);
    void setup_ssl(proton::container& cont);
};

// A thread-safe receiving connection
class receiver : private proton::messaging_handler {
public:
    receiver(proton::container& cont, const std::string& url, const std::string& address);
    proton::message receive();
    void close();

private:
    static const size_t MAX_BUFFER = 100;
    
    proton::receiver receiver_;
    std::mutex lock_;
    proton::work_queue* work_queue_;
    std::queue<proton::message> buffer_;
    std::condition_variable can_receive_;
    bool closed_;
    std::string address_;

    // Handler methods
    void on_receiver_open(proton::receiver& r) override;
    void on_message(proton::delivery& d, proton::message& m) override;
    void on_error(const proton::error_condition& e) override;
    
    void receive_done();
};

// Optional: You might want to add a convenience class that combines both sender and receiver
class amqp_client {
public:
    amqp_client(proton::container& cont, const std::string& url, const std::string& address) 
        : sender_(cont, url, address), receiver_(cont, url, address) {}
    
    void send(const proton::message& m) { sender_.send(m); }
    proton::message receive() { return receiver_.receive(); }
    void close() { 
        sender_.close(); 
        receiver_.close(); 
    }

private:
    sender sender_;
    receiver receiver_;
};

#endif // AMQP_CLIENT_HPP 