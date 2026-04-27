#pragma once

#include <atomic>
#include <cstdint>
#include <cmath>
#include <chrono>

struct RiskLimits {
    double max_position_size = 1.0;
    double max_order_size = 0.1;
    double max_daily_loss = 10000.0;
    double max_slippage = 0.001;
    int max_orders_per_second = 100;
};

class RiskManager {
public:
    RiskManager() : last_reset_ns_(0) {}

    void reset() {
        daily_pnl_.store(0.0);
        open_position_.store(0.0);
        orders_today_.store(0);
        last_reset_ns_ = get_time_ns();
    }

    bool check_order(double size, double price) const {
        if (size > limits_.max_order_size) return false;
        if (std::abs(open_position_.load() + size) > limits_.max_position_size) return false;
        if (price <= 0) return false;
        if (daily_pnl_.load() < -limits_.max_daily_loss) return false;
        return true;
    }

    void on_fill(double size, double price) {
        double notional = size * price;
        open_position_.fetch_add(size);
        if (size > 0) {
            daily_pnl_.fetch_add(notional);
        } else {
            daily_pnl_.fetch_sub(-notional);
        }
        orders_today_.fetch_add(1);
    }

    double daily_pnl() const { return daily_pnl_.load(); }
    double position() const { return open_position_.load(); }
    int orders_today() const { return orders_today_.load(); }

    void set_limits(const RiskLimits& limits) { limits_ = limits; }
    const RiskLimits& limits() const { return limits_; }

private:
    static uint64_t get_time_ns() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }

    RiskLimits limits_;
    std::atomic<double> daily_pnl_{0.0};
    std::atomic<double> open_position_{0.0};
    std::atomic<int> orders_today_{0};
    uint64_t last_reset_ns_{0};
};