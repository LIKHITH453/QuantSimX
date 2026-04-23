#include "DashboardUI.h"
#include <algorithm>
#include <cmath>
#include <numeric>

/**
 * INTEGRATION EXAMPLE: How to populate the professional dashboard metrics
 * 
 * Add these functions to your main trading loop or metrics calculation module
 */

// ===== EXAMPLE: Calculate Daily Drawdown =====
// Maximum peak-to-trough decline during the trading day
double calculate_daily_drawdown(const std::deque<double>& pnl_curve) {
    if (pnl_curve.empty()) return 0.0;
    
    double max_drawdown = 0.0;
    double running_max = pnl_curve[0];
    
    for (double pnl : pnl_curve) {
        running_max = std::max(running_max, pnl);
        double drawdown = running_max - pnl;
        max_drawdown = std::max(max_drawdown, drawdown);
    }
    
    return max_drawdown;
}

// ===== EXAMPLE: Calculate Sharpe Ratio =====
// Risk-adjusted return metric (higher is better)
// Formula: (Mean Return - Risk-Free Rate) / Std Dev of Returns
double calculate_sharpe_ratio(
    const std::vector<double>& returns,
    double risk_free_rate = 0.02  // 2% annual rate
) {
    if (returns.size() < 2) return 0.0;
    
    // Calculate mean return
    double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) 
                        / returns.size();
    
    // Calculate standard deviation of returns
    double variance = 0.0;
    for (double ret : returns) {
        variance += (ret - mean_return) * (ret - mean_return);
    }
    variance /= returns.size();
    double std_dev = std::sqrt(variance);
    
    if (std_dev == 0.0) return 0.0;
    
    // Annualize: assuming daily returns, multiply by sqrt(252 trading days)
    double sharpe = (mean_return - risk_free_rate) / std_dev * std::sqrt(252.0);
    return sharpe;
}

// ===== EXAMPLE: Calculate Win Rate =====
double calculate_win_rate(int winning_trades, int total_trades) {
    if (total_trades == 0) return 0.0;
    return (double)winning_trades / total_trades * 100.0;
}

// ===== EXAMPLE: Integration in your main loop =====
void update_dashboard_metrics(
    DashboardUI& dashboard,
    const std::deque<double>& pnl_history,
    const std::vector<double>& daily_returns,
    int winning_trades,
    int total_trades,
    uint32_t network_latency_ms
) {
    // Update metrics every frame
    dashboard.daily_drawdown_ = calculate_daily_drawdown(pnl_history);
    dashboard.sharpe_ratio_ = calculate_sharpe_ratio(daily_returns);
    dashboard.network_latency_ms_ = network_latency_ms;
    
    // These are already being set elsewhere, but for reference:
    // dashboard.cumulative_pnl_ = (already set)
    // dashboard.winning_trades_ = winning_trades;
    // dashboard.total_trades_ = total_trades ;
}

// ===== EXAMPLE: Update active position from market data =====
void update_active_position_metrics(
    DashboardUI& dashboard,
    double current_market_price,
    const ActivePosition& pos
) {
    // Calculate unrealized P&L
    dashboard.active_position_.current_price = current_market_price;
    
    // For long positions (BUY):
    if (pos.signal_type == "BUY") {
        dashboard.active_position_.pnl = (current_market_price - pos.entry_price) 
                                         * pos.quantity;
        dashboard.active_position_.pnl_percent = 
            ((current_market_price - pos.entry_price) / pos.entry_price) * 100.0;
    }
    // For short positions (SELL):
    else if (pos.signal_type == "SELL") {
        dashboard.active_position_.pnl = (pos.entry_price - current_market_price) 
                                         * pos.quantity;
        dashboard.active_position_.pnl_percent = 
            ((pos.entry_price - current_market_price) / pos.entry_price) * 100.0;
    }
    
    // Update sparkline (called automatically in render)
    // But if you want to update it manually:
    dashboard.update_position_sparkline(current_market_price);
}

// ===== EXAMPLE: Network latency measurement =====
// Measure round-trip time to exchange
uint32_t measure_network_latency(
    std::chrono::high_resolution_clock::time_point send_time
) {
    auto receive_time = std::chrono::high_resolution_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
        receive_time - send_time
    );
    return static_cast<uint32_t>(latency.count());
}

// ===== EXAMPLE: Main trading loop integration =====
/*
void main_trading_loop() {
    DashboardUI& dashboard = DashboardUI::get();
    
    while (trading_active) {
        // Get current market data
        double current_price = get_current_market_price();
        
        // Update active position metrics
        if (dashboard.has_active_position_) {
            update_active_position_metrics(dashboard, current_price, 
                                          dashboard.active_position_);
        }
        
        // Update dashboard statistics every second
        static auto last_update = std::chrono::high_resolution_clock::now();
        auto now = std::chrono::high_resolution_clock::now();
        if ((now - last_update).count() > 1000000000) {  // 1 second
            
            // Collect P&L history
            std::deque<double> pnl_curve;
            for (const auto& pos : dashboard.closed_positions_) {
                pnl_curve.push_back(dashboard.cumulative_pnl_);  // Simplified
            }
            
            // Collect daily returns
            std::vector<double> daily_returns;
            // ... calculate returns from your trade data
            
            // Update dashboard metrics
            update_dashboard_metrics(
                dashboard,
                pnl_curve,
                daily_returns,
                dashboard.winning_trades_,
                dashboard.total_trades_,
                last_recorded_latency
            );
            
            last_update = now;
        }
        
        // Render frame
        dashboard.render_frame(order_book, vwap_calc);
        
        // Sleep briefly
        std::this_thread::sleep_for(std::chrono::milliseconds(16));  // ~60 FPS
    }
}
*/

// ===== HELPER: Color utilities for custom styling =====
namespace TradingUI {
    
    // Get color based on P&L value
    ImVec4 get_pnl_color(double pnl_value, bool with_glow = false) {
        if (pnl_value > 0) {
            // Green with optional glow
            return with_glow 
                ? ImVec4(0.1f, 0.95f, 0.4f, 0.9f)  // Brighter for glow
                : ImVec4(0.0f, 0.8f, 0.3f, 1.0f);   // Standard green
        } else if (pnl_value < 0) {
            // Red
            return ImVec4(1.0f, 0.3f, 0.2f, 1.0f);
        } else {
            // Neutral
            return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
        }
    }
    
    // Get latency color based on milliseconds
    ImVec4 get_latency_color(uint32_t latency_ms) {
        if (latency_ms < 50) return ImVec4(0.1f, 0.95f, 0.4f, 1.0f);      // Green
        if (latency_ms < 100) return ImVec4(1.0f, 0.8f, 0.2f, 1.0f);     // Orange
        return ImVec4(1.0f, 0.3f, 0.2f, 1.0f);                           // Red
    }
    
    // Get drawdown color (always warning-ish)
    ImVec4 get_drawdown_color(double drawdown_pct) {
        if (drawdown_pct < 1.0) return ImVec4(0.1f, 0.95f, 0.4f, 1.0f);  // Good
        if (drawdown_pct < 5.0) return ImVec4(1.0f, 0.8f, 0.2f, 1.0f);   // Caution
        return ImVec4(1.0f, 0.3f, 0.2f, 1.0f);                           // Alert
    }
    
    // Get Sharpe ratio color
    ImVec4 get_sharpe_color(double sharpe) {
        if (sharpe > 2.0) return ImVec4(0.0f, 1.0f, 0.5f, 1.0f);         // Excellent
        if (sharpe > 1.0) return ImVec4(0.1f, 0.8f, 0.9f, 1.0f);         // Good
        if (sharpe > 0.5) return ImVec4(1.0f, 0.8f, 0.2f, 1.0f);         // Fair
        return ImVec4(1.0f, 0.3f, 0.2f, 1.0f);                           // Poor
    }
}

// ===== OPTIMIZATION: Efficient history tracking =====
class MetricsBuffer {
private:
    static const size_t BUFFER_SIZE = 1000;
    std::deque<double> pnl_buffer_;
    std::deque<double> return_buffer_;
    
public:
    void add_pnl_sample(double pnl) {
        if (pnl_buffer_.size() >= BUFFER_SIZE) {
            pnl_buffer_.pop_front();
        }
        pnl_buffer_.push_back(pnl);
    }
    
    void add_return_sample(double ret) {
        if (return_buffer_.size() >= BUFFER_SIZE) {
            return_buffer_.pop_front();
        }
        return_buffer_.push_back(ret);
    }
    
    double get_daily_drawdown() const {
        return calculate_daily_drawdown(pnl_buffer_);
    }
    
    double get_sharpe_ratio() const {
        std::vector<double> returns(return_buffer_.begin(), return_buffer_.end());
        return calculate_sharpe_ratio(returns);
    }
    
    const std::deque<double>& get_pnl_history() const {
        return pnl_buffer_;
    }
};

#endif
