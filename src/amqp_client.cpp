#include "amqp_client.hpp"
#include "ssl_utils.hpp"
#include <iostream>
#include <proton/connection_options.hpp>
#include <proton/container.hpp>
#include <proton/receiver_options.hpp>
#include <proton/sender_options.hpp>
#include <proton/source_options.hpp>
#include <proton/target.hpp>
#include <proton/target_options.hpp>
#include <proton/transport.hpp>
#include <proton/work_queue.hpp>
#include <spdlog/spdlog.h>

// Lock output from threads to avoid scrambling
std::mutex out_lock;
#define OUT(x)                                                                                                         \
	do {                                                                                                               \
		std::lock_guard<std::mutex> l(out_lock);                                                                       \
		x;                                                                                                             \
	} while (false)

// Sender implementation
sender::sender(proton::container& cont, const std::string& url, const std::string& address, const std::string& name) :
  work_queue_(0),
  queued_(0),
  credit_(0),
  address_(address) {
	proton::sender_options so;
	so.target(proton::target_options().address(address))
	  .source(proton::source_options().address(name + "-source"))
	  .handler(*this);

	cont.open_sender(url, so);
}

void sender::send(const proton::message& m) {
	{
		std::unique_lock<std::mutex> l(lock_);
		while (!work_queue_ || queued_ >= credit_)
			sender_ready_.wait(l);
		++queued_;
	}
	work_queue_->add([=]() { this->do_send(m); });
}

void sender::close() {
	work_queue()->add([=]() { sender_.connection().close(); });
}

proton::work_queue* sender::work_queue() {
	std::unique_lock<std::mutex> l(lock_);
	while (!work_queue_)
		sender_ready_.wait(l);
	return work_queue_;
}

void sender::on_sender_open(proton::sender& s) {
	std::lock_guard<std::mutex> l(lock_);
	sender_		= s;
	work_queue_ = &s.work_queue();

	spdlog::info("Client sender opened.");
	spdlog::info("  Target address: '{}'", s.target().address());
	spdlog::info("  Target dynamic: {}", s.target().dynamic());
	spdlog::info("  Connection target: '{}'", s.connection().container_id());
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
	spdlog::error("Unexpected error: {}", e.what());
	exit(1);
}

// Receiver implementation
receiver::receiver(proton::container& cont,
				   const std::string& url,
				   const std::string& address,
				   const std::string& name) :
  work_queue_(0),
  closed_(false),
  address_(address) {
	spdlog::info("Creating receiver with URL: {} and address: {}", url, address);

	proton::receiver_options ro;
	ro.credit_window(10)
	  .auto_accept(true)
	  .source(proton::source_options().address(address))
	  .target(proton::target_options().address(name + "-target"))
	  .handler(*this);

	cont.open_receiver(url, ro);
}

proton::message receiver::receive() {
	std::unique_lock<std::mutex> l(lock_);
	spdlog::debug("Waiting for message...");

	while (!closed_ && (!work_queue_ || buffer_.empty())) {
		can_receive_.wait(l);
		spdlog::debug("Woke up from wait! closed_: {}, work_queue_: {}, buffer size: {}",
					  closed_,
					  (work_queue_ != nullptr),
					  buffer_.size());
	}
	if (closed_)
		throw closed("receiver closed");
	if (buffer_.empty()) {
		spdlog::debug("Buffer is empty after wait!");
		throw std::runtime_error("No message available");
	}
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
	spdlog::info("Receiver connected on address: {}", address_);
}

void receiver::on_message(proton::delivery& d, proton::message& m) {
	{
		std::lock_guard<std::mutex> l(lock_);
		buffer_.push(m);
		spdlog::debug("Message pushed to buffer. New buffer size: {}", buffer_.size());
		can_receive_.notify_all();
		spdlog::debug("Notified waiting threads");
	}
}

void receiver::receive_done() {
	receiver_.add_credit(1);
}

void receiver::on_error(const proton::error_condition& e) {
	spdlog::error("Unexpected error: {}", e.what());
	exit(1);
}
