#include "OrderBook.h"
#include <iostream>
#include <iomanip>

OrderBook::OrderBook() = default;

void OrderBook::init_from_snapshot(const json& snapshot_json) {
    try {
        bids_.clear();
        asks_.clear();
        
        // Parse bids: [[price, qty], ...]
        if (snapshot_json.contains("bids")) {
            for (const auto& level : snapshot_json["bids"]) {
                double price = std::stod(level[0].get<std::string>());
                double qty = std::stod(level[1].get<std::string>());
                if (qty > 0) bids_[price] = qty;
            }
        }
        
        // Parse asks: [[price, qty], ...]
        if (snapshot_json.contains("asks")) {
            for (const auto& level : snapshot_json["asks"]) {
                double price = std::stod(level[0].get<std::string>());
                double qty = std::stod(level[1].get<std::string>());
                if (qty > 0) asks_[price] = qty;
            }
        }
        
        std::cout << "[OrderBook] initialized from snapshot: " << bids_.size() << " bids, "
                  << asks_.size() << " asks\n";
    } catch (const std::exception& e) {
        std::cerr << "[OrderBook] init_from_snapshot error: " << e.what() << "\n";
    }
}

void OrderBook::apply_depth_update(const json& depth_msg) {
    try {
        // Depth message format: {"b": [[price, qty], ...], "a": [[price, qty], ...], "U": lastUpdateId, ...}
        if (depth_msg.contains("b")) {
            for (const auto& level : depth_msg["b"]) {
                double price = std::stod(level[0].get<std::string>());
                double qty = std::stod(level[1].get<std::string>());
                if (qty == 0) {
                    bids_.erase(price);
                } else {
                    bids_[price] = qty;
                }
            }
        }
        
        if (depth_msg.contains("a")) {
            for (const auto& level : depth_msg["a"]) {
                double price = std::stod(level[0].get<std::string>());
                double qty = std::stod(level[1].get<std::string>());
                if (qty == 0) {
                    asks_.erase(price);
                } else {
                    asks_[price] = qty;
                }
            }
        }
        
        if (depth_msg.contains("U")) {
            last_update_id_ = depth_msg["U"].get<uint64_t>();
        }
    } catch (const std::exception& e) {
        std::cerr << "[OrderBook] apply_depth_update error: " << e.what() << "\n";
    }
}

void OrderBook::apply_aggtrade(const json& trade_msg) {
    try {
        // AggTrade message: {p: price, q: qty, T: timestamp, ...}
        if (trade_msg.contains("p") && trade_msg.contains("q")) {
            last_trade_price_ = std::stod(trade_msg["p"].get<std::string>());
            last_trade_qty_ = std::stod(trade_msg["q"].get<std::string>());
            if (trade_msg.contains("T")) {
                last_trade_time_ = trade_msg["T"].get<uint64_t>();
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[OrderBook] apply_aggtrade error: " << e.what() << "\n";
    }
}

double OrderBook::get_best_bid() const {
    return bids_.empty() ? 0.0 : bids_.begin()->first;
}

double OrderBook::get_best_ask() const {
    return asks_.empty() ? 0.0 : asks_.begin()->first;
}

double OrderBook::get_mid_price() const {
    double bid = get_best_bid();
    double ask = get_best_ask();
    return (bid > 0 && ask > 0) ? (bid + ask) / 2.0 : 0.0;
}

std::vector<OrderBookLevel> OrderBook::get_bids(int n) const {
    std::vector<OrderBookLevel> result;
    int count = 0;
    for (const auto& [price, qty] : bids_) {
        if (count++ >= n) break;
        result.push_back({price, qty});
    }
    return result;
}

std::vector<OrderBookLevel> OrderBook::get_asks(int n) const {
    std::vector<OrderBookLevel> result;
    int count = 0;
    for (const auto& [price, qty] : asks_) {
        if (count++ >= n) break;
        result.push_back({price, qty});
    }
    return result;
}

void OrderBook::print_snapshot() const {
    std::cout << "\n=== OrderBook Snapshot ===\n";
    std::cout << "Best bid: " << std::fixed << std::setprecision(2) << get_best_bid() << "\n";
    std::cout << "Best ask: " << get_best_ask() << "\n";
    std::cout << "Mid price: " << get_mid_price() << "\n";
    
    std::cout << "\nTop 5 Bids:\n";
    auto bids = get_bids(5);
    for (const auto& level : bids) {
        std::cout << "  " << level.price << " x " << level.quantity << "\n";
    }
    
    std::cout << "\nTop 5 Asks:\n";
    auto asks = get_asks(5);
    for (const auto& level : asks) {
        std::cout << "  " << level.price << " x " << level.quantity << "\n";
    }
    std::cout << "========================\n\n";
}
