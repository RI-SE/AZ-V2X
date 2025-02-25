#pragma once

#include "denm_message.hpp"
#include <atomic>
#include <crow.h>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <nlohmann/json.hpp>

class DenmService {
public:
	DenmService(
				const std::string& http_host,
				int http_port,
				int ws_port);

	~DenmService();

	void start();
	void stop();

private:
	void handleDenmPost(const crow::request& req, crow::response& res);
	void setupRoutes();

	void broadcastMessage(const std::string& message);
	void runReceiverLoop();

	void run_http_server();
	void broadcast_denm(const crow::json::rvalue& denm_message);


	std::string http_host_;
	int http_port_;
	int ws_port_;


	std::atomic<bool> running_{false};

	crow::App<> app_; // Crow application instance
	std::mutex ws_connections_mutex_;
	// Container of active websocket connections
	std::set<crow::websocket::connection*> ws_connections_;

	std::thread http_thread_;
	std::thread ws_thread_;


};
