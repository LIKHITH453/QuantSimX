#pragma once

#include <string>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct Config {
    size_t vwap_sma_period = 20;
    size_t rsi_period = 14;
    double rsi_oversold = 30.0;
    double rsi_overbought = 70.0;
    int data_collection_timeout_seconds = 180;
    int websocket_reconnect_max_attempts = 5;
    int websocket_reconnect_delay_ms = 1000;
    int dashboard_width = 1600;
    int dashboard_height = 900;
    std::string symbol = "btcusdt";
    std::string csv_output_file = "market_data.csv";
    
    static Config load(const std::string& path = "config.json") {
        Config cfg;
        try {
            std::ifstream f(path);
            if (f.is_open()) {
                json j;
                f >> j;
                if (j.contains("vwap_sma_period")) cfg.vwap_sma_period = j["vwap_sma_period"];
                if (j.contains("rsi_period")) cfg.rsi_period = j["rsi_period"];
                if (j.contains("rsi_oversold")) cfg.rsi_oversold = j["rsi_oversold"];
                if (j.contains("rsi_overbought")) cfg.rsi_overbought = j["rsi_overbought"];
                if (j.contains("data_collection_timeout_seconds")) cfg.data_collection_timeout_seconds = j["data_collection_timeout_seconds"];
                if (j.contains("websocket_reconnect_max_attempts")) cfg.websocket_reconnect_max_attempts = j["websocket_reconnect_max_attempts"];
                if (j.contains("websocket_reconnect_delay_ms")) cfg.websocket_reconnect_delay_ms = j["websocket_reconnect_delay_ms"];
                if (j.contains("dashboard_width")) cfg.dashboard_width = j["dashboard_width"];
                if (j.contains("dashboard_height")) cfg.dashboard_height = j["dashboard_height"];
                if (j.contains("symbol")) cfg.symbol = j["symbol"];
                if (j.contains("csv_output_file")) cfg.csv_output_file = j["csv_output_file"];
            }
        } catch (...) {
        }
        return cfg;
    }
};