#include "SignalGenerator.h"
#include <sstream>
#include <iomanip>

SignalGenerator::SignalGenerator(double rsi_oversold, double rsi_overbought)
    : rsi_oversold_(rsi_oversold), rsi_overbought_(rsi_overbought) {}

Signal SignalGenerator::update(uint64_t timestamp_ms, double price, double vwap, double rsi) {
    Signal signal = Signal::NONE;
    std::string reason;
    
    // Determine current state
    bool price_above = price > vwap;
    bool rsi_oversold = rsi < rsi_oversold_;
    bool rsi_overbought = rsi > rsi_overbought_;
    
    // Detect BUY signals
    // 1. Price crosses VWAP from below
    if (price_above && !price_above_vwap_ && prev_price_ > 0) {
        signal = Signal::BUY;
        reason = "Price crossed VWAP upward";
    }
    // 2. RSI exits oversold zone from below
    else if (rsi > rsi_oversold_ && rsi_in_oversold_) {
        signal = Signal::BUY;
        reason = "RSI exited oversold zone";
    }
    
    // Detect SELL signals
    // 1. Price crosses VWAP from above
    if (!price_above && price_above_vwap_ && prev_price_ > 0) {
        signal = Signal::SELL;
        reason = "Price crossed VWAP downward";
    }
    // 2. RSI exits overbought zone from above
    else if (rsi < rsi_overbought_ && rsi_in_overbought_) {
        signal = Signal::SELL;
        reason = "RSI exited overbought zone";
    }
    
    // Update state tracking
    price_above_vwap_ = price_above;
    rsi_in_oversold_ = rsi_oversold;
    rsi_in_overbought_ = rsi_overbought;
    prev_price_ = price;
    prev_vwap_ = vwap;
    prev_rsi_ = rsi;
    
    // Record signal if generated
    if (signal != Signal::NONE) {
        SignalEvent event{timestamp_ms, signal, reason, price, vwap, rsi};
        last_signal_ = event;
        signals_.push_back(event);
    }
    
    return signal;
}

std::string SignalGenerator::get_state_string() const {
    std::ostringstream oss;
    oss << "Price " << (price_above_vwap_ ? "above" : "below") << " VWAP | ";
    if (rsi_in_oversold_) {
        oss << "RSI oversold";
    } else if (rsi_in_overbought_) {
        oss << "RSI overbought";
    } else {
        oss << "RSI neutral";
    }
    return oss.str();
}
