#pragma once

#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include "OrderBook.h"

using json = nlohmann::json;
typedef websocketpp::client<websocketpp::config::asio_tls_client> WebSocketClient;
typedef websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context> context_ptr;

class BinanceWebSocketClient {
public:
    explicit BinanceWebSocketClient(OrderBook& ob);
    ~BinanceWebSocketClient();
    
    void connect(const std::string& symbol = "btcusdt");
    void disconnect();
    bool is_connected() const { return connected_; }
    
private:
    void on_open(WebSocketClient* c, websocketpp::connection_hdl hdl);
    void on_close(WebSocketClient* c, websocketpp::connection_hdl hdl);
    void on_message(WebSocketClient* c, websocketpp::connection_hdl hdl, WebSocketClient::message_ptr msg);
    void on_fail(WebSocketClient* c, websocketpp::connection_hdl hdl);
    
    context_ptr on_tls_init(websocketpp::connection_hdl hdl);
    void run_client();
    
    OrderBook& ob_;
    WebSocketClient client_;
    std::thread client_thread_;
    std::atomic<bool> connected_ = false;
    std::atomic<bool> running_ = false;
};
