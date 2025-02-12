#include "amqp_client.hpp"
#include "ssl_utils.hpp"
#include <proton/connection_options.hpp>
#include <proton/container.hpp>
#include <proton/receiver_options.hpp>
#include <proton/work_queue.hpp>
#include <proton/transport.hpp>
#include <proton/source_options.hpp>
#include <proton/target.hpp>
#include <proton/target_options.hpp>
#include <proton/sender_options.hpp>
#include <proton/connection_options.hpp>
#include <iostream>

// Lock output from threads to avoid scrambling
std::mutex out_lock;
#define OUT(x) do { std::lock_guard<std::mutex> l(out_lock); x; } while (false)

// Sender implementation
sender::sender(proton::container& cont, const std::string& url, const std::string& address)
    : work_queue_(0), queued_(0), credit_(0), address_(address)
{
    proton::sender_options so;
    so.target(proton::target_options().address(address))
      .source(proton::source_options().address(address));
 
    proton::connection_options co;
    co.user("guest")
      .password("guest")
      .handler(*this)
      .sasl_enabled(true)
      .sasl_allowed_mechs("PLAIN ANONYMOUS"); // Explicitly specify PLAIN authentication

    cont.open_sender(url, so, co);

}

void sender::send(const proton::message& m) {
    {
        std::unique_lock<std::mutex> l(lock_);
        while (!work_queue_ || queued_ >= credit_) sender_ready_.wait(l);
        ++queued_;
    }
    work_queue_->add([=]() { this->do_send(m); });
}

void sender::close() {
    work_queue()->add([=]() { sender_.connection().close(); });
}

proton::work_queue* sender::work_queue() {
    std::unique_lock<std::mutex> l(lock_);
    while (!work_queue_) sender_ready_.wait(l);
    return work_queue_;
}

void sender::setup_ssl(proton::container& cont) {
    ssl_certificate client_cert = platform_certificate("client", "");
    std::string server_CA = platform_CA("ca");
    proton::ssl_client_options ssl_cli(client_cert, server_CA, proton::ssl::VERIFY_PEER);
    proton::connection_options client_opts;
    client_opts.ssl_client_options(ssl_cli).sasl_allowed_mechs("EXTERNAL");
    cont.client_connection_options(client_opts);
}

void sender::on_sender_open(proton::sender& s) {
    std::lock_guard<std::mutex> l(lock_);
    sender_ = s;
    work_queue_ = &s.work_queue();

    std::cout << "client sender opened."
            << "\n  Target address: '" << s.target().address() << "'"
            << "\n  Target dynamic: " << s.target().dynamic()
            << "\n  Connection target: '" << s.connection().container_id() << "'"
            << std::endl;
}

void sender::on_sendable(proton::sender& s) {
    std::lock_guard<std::mutex> l(lock_);
    credit_ = s.credit();
    sender_ready_.notify_all();
}

void sender::do_send(const proton::message& m) {
    sender_.send(m);
    std::lock_guard<std::mutex> l(lock_);
    --queued_;
    credit_ = sender_.credit();
    sender_ready_.notify_all();
}

void sender::on_error(const proton::error_condition& e) {
    OUT(std::cerr << "unexpected error: " << e << std::endl);
    exit(1);
}

// Receiver implementation
receiver::receiver(proton::container& cont, const std::string& url, const std::string& address)
    : work_queue_(0), closed_(false), address_(address)
{
    
    std::cout << "Creating receiver with URL: " << url << " and address: " << address << std::endl;
    
    proton::receiver_options ro;
    ro.credit_window(10)
      .auto_accept(true)
      .source(proton::source_options().address(address))
      .target(proton::target_options().address(address));

    proton::connection_options co;
    co.user("guest")
      .password("guest")
      .handler(*this)
      .sasl_enabled(true)
      .sasl_allowed_mechs("PLAIN ANONYMOUS"); // Explicitly specify PLAIN authentication

    cont.open_receiver(url,
                      ro,
                      co);
}

void receiver::setup_ssl(proton::container& cont) {
    ssl_certificate client_cert = platform_certificate("client", "");
    std::string server_CA = platform_CA("ca");
    proton::ssl_client_options ssl_cli(client_cert, server_CA, proton::ssl::VERIFY_PEER);
    proton::connection_options client_opts;
    client_opts.ssl_client_options(ssl_cli).sasl_allowed_mechs("EXTERNAL");
    cont.client_connection_options(client_opts);
}

proton::message receiver::receive() {
    std::unique_lock<std::mutex> l(lock_);
    std::cout << "Waiting for message..." << std::endl;  // Debug
              
    while (!closed_ && (!work_queue_ || buffer_.empty())) {
        std::cout << "About to wait on condition..." << std::endl;  // Add this debug line
        can_receive_.wait(l);
        std::cout << "Woke up from wait! closed_: " << closed_ 
                  << ", work_queue_: " << (work_queue_ != nullptr)
                  << ", buffer size: " << buffer_.size() << std::endl;  // Add this debug line
    }
    if (closed_) throw closed("receiver closed");
    if (buffer_.empty()) {
        std::cout << "Buffer is empty after wait!" << std::endl;  // Debug
        throw std::runtime_error("No message available");
    }
    std::cout << "Message received, returning it..." << std::endl;  // Debug
    proton::message m = std::move(buffer_.front());
    buffer_.pop();
    work_queue_->add([=]() { this->receive_done(); });
    return m;
}

void receiver::close() {
    std::lock_guard<std::mutex> l(lock_);
    if (!closed_) {
        closed_ = true;
        can_receive_.notify_all();
        if (work_queue_) {
            work_queue_->add([this]() { this->receiver_.connection().close(); });
        }
    }
}

void receiver::on_receiver_open(proton::receiver& r) {
    receiver_ = r;
    std::lock_guard<std::mutex> l(lock_);
    work_queue_ = &receiver_.work_queue();
    receiver_.add_credit(MAX_BUFFER);
    can_receive_.notify_all();
    std::cout << "Receiver connected on address: " << address_ << std::endl;
}

void receiver::on_message(proton::delivery& d, proton::message& m) {
    std::cout << "on_message called with body: " << m.body() << std::endl;  // Debug
    std::cout << "Message properties: " << std::endl;  // Add detailed message debugging
    std::cout << "  - Reply to: " << m.reply_to() << std::endl;
    std::cout << "  - Subject: " << m.subject() << std::endl;
    std::cout << "  - Content type: " << m.content_type() << std::endl;
    
    {  // Add scope for lock
        std::lock_guard<std::mutex> l(lock_);
        buffer_.push(m);
        std::cout << "Message pushed to buffer. New buffer size: " << buffer_.size() << std::endl;
        can_receive_.notify_all();
        std::cout << "Notified waiting threads" << std::endl;
    }
}

void receiver::receive_done() {
    receiver_.add_credit(1);
}

void receiver::on_error(const proton::error_condition& e) {
    OUT(std::cerr << "unexpected error: " << e << std::endl);
    exit(1);
}