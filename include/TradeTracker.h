#pragma once

#include <vector>
#include <cstdint>
#include <cmath>

struct Trade {
    uint64_t timestamp_ms = 0;
    double price = 0.0;
    double quantity = 0.0;
    
    double notional() const { return price * quantity; }
};

class TradeTracker {
public:
    void add_trade(uint64_t timestamp_ms, double price, double qty);
    
    /// Get all trades in the window
    const std::vector<Trade>& get_trades() const { return trades_; }
    
    /// Get trades within last N milliseconds
    std::vector<Trade> get_trades_since(uint64_t time_ago_ms) const;
    
    /// Total volume in window
    double total_volume() const;
    
    /// Total notional (price * qty sum) in window
    double total_notional() const;
    
    /// Clear old trades beyond max window size
    void prune(size_t max_trades = 10000);
    
private:
    std::vector<Trade> trades_;
};
