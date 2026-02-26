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

class SignalGenerator {
public:
    SignalGenerator(double rsi_oversold = 30.0, double rsi_overbought = 70.0);
    
    /// Update signal generation with current market state
    /// Returns the generated signal (if any)
    Signal update(uint64_t timestamp_ms, double price, double vwap, double rsi);
    
    /// Get the last signal event
    const SignalEvent& last_signal() const { return last_signal_; }
    
    /// Get all signals generated so far
    const std::vector<SignalEvent>& get_signals() const { return signals_; }
    
    /// Get current state description
    std::string get_state_string() const;
    
private:
    double rsi_oversold_;
    double rsi_overbought_;
    
    // Track previous state for crossover detection
    double prev_price_ = 0.0;
    double prev_vwap_ = 0.0;
    double prev_rsi_ = 50.0;
    bool price_above_vwap_ = false;
    bool rsi_in_oversold_ = false;
    bool rsi_in_overbought_ = false;
    
    SignalEvent last_signal_;
    std::vector<SignalEvent> signals_;
};
