#pragma once

#include <atomic>
#include <cstdint>
#include <cmath>
#include <chrono>
#include <mutex>

struct RiskLimits {
    int64_t max_position_size_cents = 100000;
    int64_t max_order_size_cents = 10000;
    int64_t max_daily_loss_cents = 1000000;
    int64_t max_slippage_bps = 10;
    int max_orders_per_second = 100;
};

class RiskManager {
public:
    RiskManager() : daily_pnl_cents_(0), open_position_cents_(0), orders_today_(0), last_reset_ns_(0) {}

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        daily_pnl_cents_.store(0);
        open_position_cents_.store(0);
        orders_today_.store(0);
        last_reset_ns_ = get_time_ns();
    }

    bool check_order(int64_t size_cents, int64_t price_cents) const {
        if (size_cents > limits_.max_order_size_cents) return false;
        if (std::abs(open_position_cents_.load() + size_cents) > limits_.max_position_size_cents) return false;
        if (price_cents <= 0) return false;
        if (daily_pnl_cents_.load() < -limits_.max_daily_loss_cents) return false;
        return true;
    }

    void on_fill(int64_t size_cents, int64_t price_cents) {
        std::lock_guard<std::mutex> lock(mutex_);
        int64_t notional = size_cents * price_cents / 10000;
        open_position_cents_.fetch_add(size_cents);
        daily_pnl_cents_.fetch_add(notional);
        orders_today_.fetch_add(1);
    }

    int64_t daily_pnl_cents() const { return daily_pnl_cents_.load(); }
    int64_t position_cents() const { return open_position_cents_.load(); }
    int orders_today() const { return orders_today_.load(); }

    void set_limits(const RiskLimits& limits) { limits_ = limits; }
    const RiskLimits& limits() const { return limits_; }

private:
    static uint64_t get_time_ns() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }

    RiskLimits limits_;
    std::atomic<int64_t> daily_pnl_cents_;
    std::atomic<int64_t> open_position_cents_;
    std::atomic<int> orders_today_;
    uint64_t last_reset_ns_;
    std::mutex mutex_;
};