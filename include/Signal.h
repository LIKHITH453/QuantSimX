#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class Signal {
    NONE = 0,
    BUY = 1,
    SELL = -1
};

struct SignalEvent {
    uint64_t timestamp_ms = 0;
    Signal signal = Signal::NONE;
    std::string reason;
    double price = 0.0;
    double vwap = 0.0;
    double rsi = 0.0;
};

struct TickData {
    uint64_t timestamp_ms = 0;
    double price = 0.0;
    double volume = 0.0;
    double bid_price = 0.0;
    double ask_price = 0.0;
    double bid_volume = 0.0;
    double ask_volume = 0.0;
    bool is_buy_tick = true;
    double vwap = 0.0;
    double rsi = 50.0;
    double sma_fast = 0.0;
    double sma_slow = 0.0;
    double macd_line = 0.0;
    double signal_line = 0.0;
    double atr = 0.0;
    double mid_price() const { return (bid_price + ask_price) * 0.5; }
};