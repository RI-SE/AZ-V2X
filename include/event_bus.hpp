#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

class EventBus {
public:
	using JsonCallback = std::function<void(const nlohmann::json&)>;

	static EventBus& getInstance() {
		static EventBus instance;
		return instance;
	}

	// Subscribe to an event
	void subscribe(const std::string& event, JsonCallback callback) {
		std::lock_guard<std::mutex> lock(mutex_);
		subscribers_[event].push_back(callback);
	}

	// Publish an event
	void publish(const std::string& event, const nlohmann::json& data) {
		std::lock_guard<std::mutex> lock(mutex_);
		if (subscribers_.find(event) != subscribers_.end()) {
			for (const auto& callback : subscribers_[event]) {
				callback(data);
			}
		}
	}

private:
	EventBus() = default;
	std::map<std::string, std::vector<JsonCallback>> subscribers_;
	std::mutex mutex_;
};
