#include "BinanceWebSocketClient.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <openssl/ssl.h>

BinanceWebSocketClient::BinanceWebSocketClient(OrderBook& ob)
    : ob_(ob) {
    
    client_.set_access_channels(websocketpp::log::alevel::all);
    client_.clear_access_channels(websocketpp::log::alevel::frame_payload);
    
    client_.init_asio();
    
    // Set TLS init handler
    client_.set_tls_init_handler([this](websocketpp::connection_hdl hdl) {
        return on_tls_init(hdl);
    });
    
    // Set message handlers
    client_.set_open_handler([this](websocketpp::connection_hdl hdl) {
        on_open(nullptr, hdl);
    });
    client_.set_close_handler([this](websocketpp::connection_hdl hdl) {
        on_close(nullptr, hdl);
    });
    client_.set_message_handler([this](websocketpp::connection_hdl hdl, WebSocketClient::message_ptr msg) {
        on_message(nullptr, hdl, msg);
    });
    client_.set_fail_handler([this](websocketpp::connection_hdl hdl) {
        on_fail(nullptr, hdl);
    });
}

BinanceWebSocketClient::~BinanceWebSocketClient() {
    running_ = false;
    // Don't call disconnect() in destructor, just let thread finish
    if (client_thread_.joinable()) {
        // Detach instead of join to avoid deadlock
        client_thread_.detach();
    }
}

context_ptr BinanceWebSocketClient::on_tls_init(websocketpp::connection_hdl hdl) {
    context_ptr ctx = websocketpp::lib::make_shared<websocketpp::lib::asio::ssl::context>(
        websocketpp::lib::asio::ssl::context::sslv23);
    ctx->set_options(
        websocketpp::lib::asio::ssl::context::default_workarounds |
        websocketpp::lib::asio::ssl::context::no_sslv2 |
        websocketpp::lib::asio::ssl::context::single_dh_use);
    return ctx;
}

void BinanceWebSocketClient::on_open(WebSocketClient* c, websocketpp::connection_hdl hdl) {
    connected_ = true;
    std::cout << "[WebSocket] Connected to Binance\n";
}

void BinanceWebSocketClient::on_close(WebSocketClient* c, websocketpp::connection_hdl hdl) {
    connected_ = false;
    std::cout << "[WebSocket] Disconnected from Binance\n";
}

void BinanceWebSocketClient::on_fail(WebSocketClient* c, websocketpp::connection_hdl hdl) {
    connected_ = false;
    std::cout << "[WebSocket] Connection failed\n";
}

void BinanceWebSocketClient::on_message(WebSocketClient* c, websocketpp::connection_hdl hdl,
                                       WebSocketClient::message_ptr msg) {
    try {
        std::string payload = msg->get_payload();
        json j = json::parse(payload);
        
        // Binance stream response wraps data in a "data" field
        if (j.contains("data")) {
            j = j["data"];
        }
        
        // Dispatch based on event type
        if (j.contains("e")) {
            std::string event = j["e"].get<std::string>();
            
            if (event == "depthUpdate") {
                ob_.apply_depth_update(j);
                std::cout << "[WebSocket] Received depth update\n";
            } else if (event == "aggTrade") {
                ob_.apply_aggtrade(j);
                std::cout << "[WebSocket] Received aggTrade\n";
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[WebSocket] Message parsing error: " << e.what() << "\n";
    }
}

void BinanceWebSocketClient::run_client() {
    try {
        client_.run();
    } catch (const std::exception& e) {
        std::cerr << "[WebSocket] Client error: " << e.what() << "\n";
    }
    running_ = false;
}

void BinanceWebSocketClient::connect(const std::string& symbol) {
    if (running_) return;
    
    try {
        running_ = true;
        
        // Build WebSocket URL for Spot market with dual streams
        std::string url = "wss://stream.binance.com:9443/stream?streams=";
        url += symbol + "@depth@100ms/" + symbol + "@aggTrade";
        
        websocketpp::lib::error_code ec;
        WebSocketClient::connection_ptr con = client_.get_connection(url, ec);
        
        if (ec) {
            std::cerr << "[WebSocket] Get connection error: " << ec.message() << "\n";
            running_ = false;
            return;
        }
        
        client_.connect(con);
        client_thread_ = std::thread(&BinanceWebSocketClient::run_client, this);
        
        std::cout << "[WebSocket] Connecting to " << url << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[WebSocket] Connect error: " << e.what() << "\n";
        running_ = false;
    }
}

void BinanceWebSocketClient::disconnect() {
    if (!running_) return;
    
    running_ = false;
    
    // Just set running flag to false and let the thread exit naturally
    std::cout << "[WebSocket] Disconnect initiated\n";
}
