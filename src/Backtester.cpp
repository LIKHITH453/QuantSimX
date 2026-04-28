#include "Backtester.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <map>
#include <iostream>
#include <iomanip>

Backtester::Backtester(double rsi_oversold, double rsi_overbought)
    : signal_gen_(rsi_oversold, rsi_overbought),
      vwap_calc_(20, 14) {
}

bool Backtester::load_csv(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[Backtester] Failed to open CSV: " << filepath << "\n";
        return false;
    }
    
    bars_.clear();
    std::string line;
    
    // Skip header
    std::getline(file, line);
    
    BacktestBar current_bar;
    uint64_t current_bar_time = 0;
    uint32_t bar_counter = 0;
    
    // Aggregate 5-second bars from tick data
    const uint64_t BAR_INTERVAL_MS = 5000;
    
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> fields;
        
        while (std::getline(ss, token, ',')) {
            fields.push_back(token);
        }
        
        if (fields.size() < 10) continue;
        
        try {
            uint64_t ts = std::stoull(fields[0]);
            std::string event = fields[1];
            
            if (event != "trade") continue;
            
            double bid = std::stod(fields[2]);
            double ask = std::stod(fields[3]);
            double mid = std::stod(fields[4]);
            double price = std::stod(fields[5]);
            double qty = std::stod(fields[6]);
            double vwap = std::stod(fields[7]);
            double sma = std::stod(fields[8]);
            double rsi = std::stod(fields[9]);
            
            // Determine bar time
            uint64_t bar_time = (ts / BAR_INTERVAL_MS) * BAR_INTERVAL_MS;
            
            if (current_bar_time == 0) {
                current_bar_time = bar_time;
                current_bar.timestamp_ms = bar_time;
                current_bar.open = price;
                current_bar.high = price;
                current_bar.low = price;
                current_bar.close = price;
                current_bar.vwap = vwap;
                current_bar.sma = sma;
                current_bar.rsi = rsi;
                current_bar.volume = qty;
            } else if (bar_time == current_bar_time) {
                // Update current bar
                current_bar.high = std::max(current_bar.high, price);
                current_bar.low = std::min(current_bar.low, price);
                current_bar.close = price;
                current_bar.vwap = vwap;
                current_bar.sma = sma;
                current_bar.rsi = rsi;
                current_bar.volume += qty;
            } else {
                // New bar, save current and start new
                bars_.push_back(current_bar);
                current_bar_time = bar_time;
                current_bar.timestamp_ms = bar_time;
                current_bar.open = price;
                current_bar.high = price;
                current_bar.low = price;
                current_bar.close = price;
                current_bar.vwap = vwap;
                current_bar.sma = sma;
                current_bar.rsi = rsi;
                current_bar.volume = qty;
            }
        } catch (const std::exception& e) {
            std::cerr << "[Backtester] Error parsing line: " << e.what() << "\n";
            continue;
        }
    }
    
    // Don't forget last bar
    if (current_bar.volume > 0) {
        bars_.push_back(current_bar);
    }
    
    std::cout << "[Backtester] Loaded " << bars_.size() << " bars from CSV\n";
    return !bars_.empty();
}

BacktestMetrics Backtester::run() {
    if (bars_.empty()) {
        std::cerr << "[Backtester] No bars loaded\n";
        return metrics_;
    }
    
    is_running_ = true;
    trades_.clear();
    
    Signal current_position = Signal::NONE;
    double entry_price = 0.0;
    uint64_t entry_time = 0;
    std::string entry_reason;
    
    std::cout << "[Backtester] Starting backtest with " << bars_.size() << " bars...\n";
    
    for (size_t i = 0; i < bars_.size(); i++) {
        const auto& bar = bars_[i];
        progress_ = static_cast<double>(i) / bars_.size();
        
        Signal sig = signal_gen_.update(bar.timestamp_ms, bar.close, bar.vwap, bar.rsi);
        
        if (sig == Signal::NONE) continue;
        
        if (current_position == Signal::NONE && sig == Signal::BUY) {
            current_position = Signal::BUY;
            entry_price = bar.close;
            entry_time = bar.timestamp_ms;
            entry_reason = signal_gen_.last_signal().reason;
        }
        else if (current_position == Signal::NONE && sig == Signal::SELL) {
            current_position = Signal::SELL;
            entry_price = bar.close;
            entry_time = bar.timestamp_ms;
            entry_reason = signal_gen_.last_signal().reason;
        }
        else if (current_position == Signal::BUY && sig == Signal::SELL) {
            BacktestTrade trade;
            trade.entry_time = entry_time;
            trade.exit_time = bar.timestamp_ms;
            trade.entry_price = entry_price;
            trade.exit_price = bar.close;
            trade.pnl = (bar.close - entry_price);
            trade.pnl_percent = (bar.close - entry_price) / entry_price * 100.0;
            trade.entry_reason = entry_reason;
            trade.exit_reason = signal_gen_.last_signal().reason;
            trades_.push_back(trade);
            current_position = Signal::NONE;
        }
        else if (current_position == Signal::SELL && sig == Signal::BUY) {
            BacktestTrade trade;
            trade.entry_time = entry_time;
            trade.exit_time = bar.timestamp_ms;
            trade.entry_price = entry_price;
            trade.exit_price = bar.close;
            trade.pnl = (entry_price - bar.close);
            trade.pnl_percent = (entry_price - bar.close) / entry_price * 100.0;
            trade.entry_reason = entry_reason;
            trade.exit_reason = signal_gen_.last_signal().reason;
            trades_.push_back(trade);
            current_position = Signal::NONE;
        }
    }
    
    if (current_position != Signal::NONE && !bars_.empty()) {
        const auto& bar = bars_.back();
        BacktestTrade trade;
        trade.entry_time = entry_time;
        trade.exit_time = bar.timestamp_ms;
        trade.entry_price = entry_price;
        trade.exit_price = bar.close;
        if (current_position == Signal::BUY) {
            trade.pnl = (bar.close - entry_price);
            trade.pnl_percent = (bar.close - entry_price) / entry_price * 100.0;
        } else {
            trade.pnl = (entry_price - bar.close);
            trade.pnl_percent = (entry_price - bar.close) / entry_price * 100.0;
        }
        trade.entry_reason = entry_reason;
        trade.exit_reason = "End of backtest";
        trades_.push_back(trade);
    }
    
    progress_ = 1.0;
    is_running_ = false;
    
    // Compute metrics
    metrics_ = compute_metrics();
    
    std::cout << "[Backtester] Backtest complete!\n";
    std::cout << "  Total trades: " << metrics_.total_trades << "\n";
    std::cout << "  Win rate: " << std::fixed << std::setprecision(1) 
              << metrics_.win_rate * 100.0 << "%\n";
    std::cout << "  Total P&L: " << std::setprecision(2) << metrics_.total_pnl << "\n";
    std::cout << "  Max drawdown: " << std::setprecision(2) << metrics_.max_drawdown * 100.0 << "%\n";
    std::cout << "  Sharpe ratio: " << std::setprecision(2) << metrics_.sharpe_ratio << "\n";
    
    return metrics_;
}

BacktestMetrics Backtester::compute_metrics() {
    BacktestMetrics m;
    m.total_trades = trades_.size();
    
    if (m.total_trades == 0) {
        return m;
    }
    
    double total_win = 0.0;
    double total_loss = 0.0;
    double running_pnl = 0.0;
    double peak_pnl = 0.0;
    int consecutive_wins = 0;
    int consecutive_losses = 0;
    
    for (const auto& trade : trades_) {
        if (trade.pnl > 0) {
            m.winning_trades++;
            total_win += trade.pnl;
            consecutive_wins++;
            consecutive_losses = 0;
        } else if (trade.pnl < 0) {
            m.losing_trades++;
            total_loss += trade.pnl;
            consecutive_losses++;
            consecutive_wins = 0;
        }
        
        running_pnl += trade.pnl;
        peak_pnl = std::max(peak_pnl, running_pnl);
        m.max_consecutive_wins = std::max(m.max_consecutive_wins, static_cast<double>(consecutive_wins));
        m.max_consecutive_losses = std::max(m.max_consecutive_losses, static_cast<double>(consecutive_losses));
    }
    
    m.win_rate = m.winning_trades / static_cast<double>(m.total_trades);
    m.total_pnl = running_pnl;
    m.max_drawdown = (peak_pnl - running_pnl) / std::max(1.0, peak_pnl);
    
    if (m.winning_trades > 0) {
        m.avg_win = total_win / m.winning_trades;
    }
    
    if (m.losing_trades > 0) {
        m.avg_loss = total_loss / m.losing_trades;
    }
    
    if (std::abs(total_loss) > 1e-6) {
        m.profit_factor = total_win / std::abs(total_loss);
    }
    
    // Compute Sharpe ratio from daily returns
    auto returns = compute_returns();
    if (!returns.empty()) {
        double mean_return = 0.0;
        for (auto r : returns) mean_return += r;
        mean_return /= returns.size();
        
        double variance = 0.0;
        for (auto r : returns) {
            variance += (r - mean_return) * (r - mean_return);
        }
        variance /= returns.size();
        double std_dev = std::sqrt(variance);
        
        if (std_dev > 1e-6) {
            // Annualize: assume 252 trading days
            m.sharpe_ratio = (mean_return / std_dev) * std::sqrt(252.0);
        }
    }
    
    return m;
}

std::vector<double> Backtester::compute_returns() const {
    std::vector<double> returns;
    
    if (trades_.empty()) return returns;
    
    // Group trades by day and compute daily returns
    std::map<uint64_t, double> daily_pnl;
    
    for (const auto& trade : trades_) {
        // Get day from timestamp
        uint64_t day = trade.exit_time / (24 * 3600 * 1000);
        daily_pnl[day] += trade.pnl;
    }
    
    for (const auto& [day, pnl] : daily_pnl) {
        returns.push_back(pnl);
    }
    
    return returns;
}
