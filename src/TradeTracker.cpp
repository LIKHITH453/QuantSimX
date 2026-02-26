#include "TradeTracker.h"

void TradeTracker::add_trade(uint64_t timestamp_ms, double price, double qty) {
    trades_.push_back({timestamp_ms, price, qty});
}

std::vector<Trade> TradeTracker::get_trades_since(uint64_t time_ago_ms) const {
    std::vector<Trade> result;
    if (trades_.empty()) return result;
    
    uint64_t cutoff = trades_.back().timestamp_ms - time_ago_ms;
    for (const auto& t : trades_) {
        if (t.timestamp_ms >= cutoff) {
            result.push_back(t);
        }
    }
    return result;
}

double TradeTracker::total_volume() const {
    double vol = 0.0;
    for (const auto& t : trades_) {
        vol += t.quantity;
    }
    return vol;
}

double TradeTracker::total_notional() const {
    double notional = 0.0;
    for (const auto& t : trades_) {
        notional += t.notional();
    }
    return notional;
}

void TradeTracker::prune(size_t max_trades) {
    if (trades_.size() > max_trades) {
        trades_.erase(trades_.begin(), trades_.begin() + (trades_.size() - max_trades));
    }
}
