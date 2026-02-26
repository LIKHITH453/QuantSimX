#include "NetworkClient.h"
#include <chrono>
#include <iostream>
#include <thread>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

NetworkClient::NetworkClient(OrderBook& ob)
    : ob_(ob)
{}

NetworkClient::~NetworkClient() {
    stop();
}

void NetworkClient::start() {
    if (running_) return;
    running_ = true;
    thread_ = std::thread(&NetworkClient::run_simulator, this);
}

void NetworkClient::stop() {
    if (!running_) return;
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

void NetworkClient::run_simulator() {
    using namespace std::chrono_literals;
    int iter = 0;
    while (running_) {
        // Simulate a depth update JSON message (stub)
        std::string depth_msg = "{\"e\":\"depthUpdate\", \"E\":123456, \"b\": [[\"60000.0\", \"0.1\"]], \"a\": [[\"60010.0\", \"0.2\"]]}";
        json depth_json = json::parse(depth_msg);
        ob_.apply_depth_update(depth_json);

        // Simulate an aggTrade JSON message (low granularity)
        std::string trade_msg = "{\"e\":\"aggTrade\", \"E\":123457, \"p\":\"60005.0\", \"q\":\"0.001\"}";
        json trade_json = json::parse(trade_msg);
        ob_.apply_aggtrade(trade_json);

        if (++iter % 10 == 0) {
            std::cout << "[NetworkClient] simulated messages sent\n";
        }

        std::this_thread::sleep_for(100ms);
    }
}
