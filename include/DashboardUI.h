#pragma once

#include <memory>
#include <deque>
#include <vector>
#include <ctime>
#include <chrono>

#include "OrderBook.h"
#include "VWAPCalculator.h"
#include "SignalGenerator.h"
#include "Backtester.h"

struct ImDrawData;

struct ActivePosition {
    uint64_t entry_time = 0;
    double entry_price = 0.0;
    double current_price = 0.0;
    double quantity = 1.0;
    double pnl = 0.0;
    double pnl_percent = 0.0;
    std::string signal_type;  // "BUY" or "SELL"
};

enum class AppState {
    IDLE,
    CONNECTING,
    CONNECTED,
    STOPPED
};

class DashboardUI {
public:
    static DashboardUI& get() {
        static DashboardUI instance;
        return instance;
    }

    bool init(int width, int height);
    void shutdown();
    bool render_frame(const OrderBook& ob, const VWAPCalculator& vwap_calc,
                     const SignalGenerator& sig_gen);

    bool should_connect() const { return should_connect_; }
    bool should_disconnect() const { return should_disconnect_; }
    bool should_stop_collection() const { return should_stop_; }
    bool should_run_backtest() const { return should_backtest_; }
    bool should_close() const { return should_close_; }
    void reset_button_flags();

    AppState get_app_state() const { return app_state_; }
    void set_app_state(AppState state);

    void set_backtest_metrics(const BacktestMetrics& metrics) {
        backtest_metrics_ = metrics;
        backtest_has_results_ = true;
    }

private:
    DashboardUI() = default;
    ~DashboardUI() = default;

    void* glfw_window_ = nullptr;
    AppState app_state_ = AppState::IDLE;
    bool should_connect_ = false;
    bool should_disconnect_ = false;
    bool should_stop_ = false;
    bool should_backtest_ = false;
    bool should_close_ = false;

    std::deque<double> price_history_;
    std::deque<double> vwap_history_;
    
    // smoothed display values to avoid flicker
    double displayed_price_ = 0.0;
    double displayed_vwap_ = 0.0;

    // smoothing parameters (configurable via UI)
    float price_smooth_alpha_ = 0.1f;
    float vwap_smooth_alpha_  = 0.1f;

    // debug controls
    bool show_metrics_window_ = false;

    // regime analysis data
    std::vector<double> analysis_returns_;
    std::vector<double> analysis_volatility_;
    std::vector<double> filtered_prob_low_;  // low-vol regime
    std::vector<double> filtered_prob_high_; // high-vol regime
    
    // Live trading dashboard state
    ActivePosition active_position_;
    bool has_active_position_ = false;
    std::deque<ActivePosition> closed_positions_;  // Trade history
    double cumulative_pnl_ = 0.0;
    int total_trades_ = 0;
    int winning_trades_ = 0;
    int losing_trades_ = 0;
    
    BacktestMetrics backtest_metrics_;
    bool backtest_has_results_ = false;
    float connection_anim_time_ = 0.0f;

    void render_top_panel(const OrderBook& ob, const VWAPCalculator& vwap);
    void render_orderbook_panel(const OrderBook& ob);
    void render_price_chart_panel();
    void render_regime_analysis_panel();
    void render_stats_panel(const VWAPCalculator& vwap);
    void render_signals_panel(const SignalGenerator& sig_gen);
    void render_trading_dashboard_panel();
    void render_backtest_results_panel();
    void render_control_panel();
    void render_debug_controls();
    
    // Live trading tracking methods
    void update_active_position(double current_price, const SignalGenerator& sig_gen);
    // Analysis computation
    void compute_regime_analysis();
};
