#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "SignalGenerator.h"
#include "VWAPCalculator.h"

struct BacktestBar {
    uint64_t timestamp_ms = 0;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    double vwap = 0.0;
    double sma = 0.0;
    double rsi = 0.0;
    double volume = 0.0;
};

struct BacktestTrade {
    uint64_t entry_time = 0;
    uint64_t exit_time = 0;
    double entry_price = 0.0;
    double exit_price = 0.0;
    double pnl = 0.0;
    double pnl_percent = 0.0;
    std::string entry_reason;
    std::string exit_reason;
};

struct BacktestMetrics {
    int total_trades = 0;
    int winning_trades = 0;
    int losing_trades = 0;
    double win_rate = 0.0;
    double total_pnl = 0.0;
    double avg_win = 0.0;
    double avg_loss = 0.0;
    double profit_factor = 0.0;  // sum of wins / abs(sum of losses)
    double max_drawdown = 0.0;
    double max_consecutive_wins = 0;
    double max_consecutive_losses = 0;
    double sharpe_ratio = 0.0;
};

class Backtester {
public:
    Backtester(double rsi_oversold = 30.0, double rsi_overbought = 70.0);
    
    /// Load CSV file and parse into bars
    /// Returns true if successful
    bool load_csv(const std::string& filepath);
    
    /// Run the backtest
    /// Returns metrics
    BacktestMetrics run();
    
    /// Get all trades from the backtest
    const std::vector<BacktestTrade>& get_trades() const { return trades_; }
    
    /// Get all bars loaded
    const std::vector<BacktestBar>& get_bars() const { return bars_; }
    
    /// Get metrics
    const BacktestMetrics& get_metrics() const { return metrics_; }
    
    /// Get state for UI display
    bool is_running() const { return is_running_; }
    double get_progress() const { return progress_; }
    
private:
    std::vector<BacktestBar> bars_;
    std::vector<BacktestTrade> trades_;
    BacktestMetrics metrics_;
    
    SignalGenerator signal_gen_;
    VWAPCalculator vwap_calc_;
    
    bool is_running_ = false;
    double progress_ = 0.0;
    
    // Helper methods
    BacktestMetrics compute_metrics();
    std::vector<double> compute_returns() const;
};
