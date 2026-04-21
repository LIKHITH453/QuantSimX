#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct OrderBookLevel {
    double price = 0.0;
    double quantity = 0.0;
};

class OrderBook {
public:
    static constexpr std::size_t MAX_LEVELS = 256;
    
    OrderBook();
    
    void init_from_snapshot(const json& snapshot_json);
    void apply_depth_update(const json& depth_msg);
    void apply_aggtrade(const json& trade_msg);
    
    double get_best_bid() const { return bids_[0].price; }
    double get_best_ask() const { return asks_[0].price; }
    double get_mid_price() const {
        double bid = get_best_bid();
        double ask = get_best_ask();
        return (bid > 0 && ask > 0) ? (bid + ask) * 0.5 : 0.0;
    }
    
    double get_last_trade_price() const { return last_trade_price_; }
    double get_last_trade_qty() const { return last_trade_qty_; }
    uint64_t get_last_trade_time() const { return last_trade_time_; }
    
    void print_snapshot() const;
    
    std::size_t get_bid_count() const { return bid_count_; }
    std::size_t get_ask_count() const { return ask_count_; }
    
    const OrderBookLevel* get_bids() const { return bids_.data(); }
    const OrderBookLevel* get_asks() const { return asks_.data(); }

private:
    void insert_bid(double price, double qty);
    void insert_ask(double price, double qty);
    void erase_bid(std::size_t idx);
    void erase_ask(std::size_t idx);
    
    alignas(64) std::array<OrderBookLevel, MAX_LEVELS> bids_;
    alignas(64) std::array<OrderBookLevel, MAX_LEVELS> asks_;
    alignas(64) std::size_t bid_count_ = 0;
    alignas(64) std::size_t ask_count_ = 0;
    
    uint64_t last_update_id_ = 0;
    double last_trade_price_ = 0.0;
    double last_trade_qty_ = 0.0;
    uint64_t last_trade_time_ = 0;
};