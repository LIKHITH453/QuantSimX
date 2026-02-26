#pragma once

#include <vector>
#include <string>
#include <map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct OrderBookLevel {
    double price = 0.0;
    double quantity = 0.0;
};

class OrderBook {
public:
    OrderBook();
    
    /// Initialize from REST snapshot (top N levels)
    void init_from_snapshot(const json& snapshot_json);
    
    /// Apply depth update from WebSocket (depth@100ms stream)
    void apply_depth_update(const json& depth_msg);
    
    /// Apply trade update from WebSocket (aggTrade stream)
    void apply_aggtrade(const json& trade_msg);
    
    /// Get bid/ask for VWAP and signal computation
    double get_best_bid() const;
    double get_best_ask() const;
    double get_mid_price() const;
    
    /// Get last trade price and quantity
    double get_last_trade_price() const { return last_trade_price_; }
    double get_last_trade_qty() const { return last_trade_qty_; }
    uint64_t get_last_trade_time() const { return last_trade_time_; }
    
    /// Print orderbook snapshot
    void print_snapshot() const;
    
    /// Get top N levels
    std::vector<OrderBookLevel> get_bids(int n = 10) const;
    std::vector<OrderBookLevel> get_asks(int n = 10) const;
    
private:
    // Use maps for O(log n) insertion/deletion and auto-sort
    // Price -> Quantity for bids (descending price)
    // Price -> Quantity for asks (ascending price)
    std::map<double, double, std::greater<double>> bids_;  // bid: higher price first
    std::map<double, double> asks_;  // ask: lower price first
    
    uint64_t last_update_id_ = 0;
    double last_trade_price_ = 0.0;
    double last_trade_qty_ = 0.0;
    uint64_t last_trade_time_ = 0;
};
