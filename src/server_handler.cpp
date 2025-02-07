#include "server_handler.hpp"
#include "ssl_utils.hpp"
#include <proton/connection.hpp>
#include <proton/transport.hpp>
#include <proton/message.hpp>
#include <proton/sasl.hpp>
#include <proton/delivery.hpp>
#include <proton/target.hpp>
#include <proton/link.hpp>
#include <proton/sender_options.hpp>
#include <proton/target_options.hpp>
#include <iostream>

void server_handler::on_connection_open(proton::connection &c) {
    if (c.transport().sasl().outcome() == proton::sasl::OK) {
        std::string subject = c.transport().ssl().remote_subject();
    } else {
        std::cout << "Inbound client authentication failed" << std::endl;
        c.close();
    }
    messaging_handler::on_connection_open(c);
}

void server_handler::on_message(proton::delivery& d, proton::message& m) {
    std::cout << "Server received: " << m.body() << std::endl;
    
    // Create a response message
    pending_response_ = proton::message();
    pending_response_.body("Hello back!");
    has_pending_response_ = true;
    
    if (!m.reply_to().empty()) {
        std::cout << "Server replying to: " << m.reply_to() << std::endl;
        
        // Create a sender for the reply if we don't have one
        if (!sender_ || sender_.target().address() != m.reply_to()) {
            std::cout << "Creating new sender for reply to: '" << m.reply_to() << "'" << std::endl;
            
            proton::sender_options so;
            so.target(proton::target_options().address(m.reply_to()));
            sender_ = d.connection().open_sender(m.reply_to(), so);
            
            std::cout << "Sender creation initiated for address: " << m.reply_to() << std::endl;
        } else {
            // If sender already exists and is ready, send immediately
            send_pending_response();
        }
    }
}

void server_handler::on_sender_open(proton::sender& s) {
    std::cout << "Server reply sender opened."
              << "\n  Target address: '" << s.target().address() << "'"
              << "\n  Target dynamic: " << s.target().dynamic()
              << "\n  Connection target: '" << s.connection().container_id() << "'"
              << std::endl;
    
    // Now that the sender is ready, send any pending response
    // send_pending_response();
}

void server_handler::on_sendable(proton::sender& s) {
    send_pending_response();
}

void server_handler::on_sender_error(proton::sender& s) {
    std::cout << "Server sender error: " << s.error() << std::endl;
}

void server_handler::send_pending_response() {
    if (has_pending_response_ && sender_) {
        try {
            sender_.send(pending_response_);
            std::cout << "Response sent successfully to target: " << sender_.target().address() << std::endl;
            has_pending_response_ = false;
        } catch (const std::exception& e) {
            std::cerr << "Error sending response: " << e.what() << std::endl;
        }
    }
}