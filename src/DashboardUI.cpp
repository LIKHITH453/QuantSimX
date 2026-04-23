#include "DashboardUI.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <chrono>

// simple debug macros for ImGui call counting
#define DBG_BEG() ++debug_begin_count
#define DBG_END() ++debug_end_count

#ifdef _WIN32
#define GL_GLEXT_PROTOTYPES
#endif

bool DashboardUI::init(int width, int height) {
    if (!glfwInit()) {
        std::cerr << "[DashboardUI] glfwInit failed\n";
        return false;
    }

    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    glfw_window_ = glfwCreateWindow(width, height, "QuantSimX - HFT Trading Platform", NULL, NULL);
    if (!glfw_window_) {
        std::cerr << "[DashboardUI] glfwCreateWindow failed\n";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent((GLFWwindow*)glfw_window_);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    
    // HFT-focused color scheme - dark with neon accents
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.08f, 1.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.1f, 0.1f, 0.15f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.15f, 0.15f, 0.2f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.2f, 0.2f, 0.25f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.25f, 0.25f, 0.3f, 1.0f);
    style.Colors[ImGuiCol_Text] = ImVec4(0.9f, 0.9f, 0.95f, 1.0f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.5f, 0.5f, 0.6f, 1.0f);

    ImGui_ImplGlfw_InitForOpenGL((GLFWwindow*)glfw_window_, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    return true;
}

void DashboardUI::shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (glfw_window_) {
        glfwDestroyWindow((GLFWwindow*)glfw_window_);
        glfwTerminate();
    }
}

bool DashboardUI::render_frame(const OrderBook& ob, const VWAPCalculator& vwap_calc,
                               const SignalGenerator& sig_gen) {
    GLFWwindow* window = (GLFWwindow*)glfw_window_;
    auto frame_start = std::chrono::high_resolution_clock::now();

    // automatic STOP trigger for debugging (200 frames)
    static int auto_frame = 0;
    if (++auto_frame == 200) {
        should_stop_ = true;
    }

    // debug counters to track Begin/End mismatches
    static int debug_begin_count = 0;
    static int debug_end_count   = 0;
    debug_begin_count = 0;
    debug_end_count   = 0;


    if (glfwWindowShouldClose(window)) {
        should_close_ = true;
        return false;
    }

    // Collect price history for chart display with smoothing for visual stability
    double mid_price = ob.get_mid_price();
    if (mid_price > 0) {
        if (displayed_price_ <= 0.0) {
            displayed_price_ = mid_price;
        } else {
            // use configurable smoothing factor
            displayed_price_ += (mid_price - displayed_price_) * price_smooth_alpha_;
        }
        price_history_.push_back(displayed_price_);
        if (price_history_.size() > 500) {
            price_history_.pop_front();
        }
        
        // Regime analysis will be computed separately when rendering the tab
    }

    // Collect VWAP history with smoothing
    double vwap = vwap_calc.get_vwap();
    if (vwap > 0) {
        if (displayed_vwap_ <= 0.0) {
            displayed_vwap_ = vwap;
        } else {
            displayed_vwap_ += (vwap - displayed_vwap_) * vwap_smooth_alpha_;
        }
        vwap_history_.push_back(displayed_vwap_);
        if (vwap_history_.size() > 500) {
            vwap_history_.pop_front();
        }
    }

    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_FirstUseEver);

    // optional debug windows
    if (show_metrics_window_) {
        ImGui::ShowMetricsWindow(&show_metrics_window_);
    }

    // main dashboard window: always call End() even if Begin returns false
    DBG_BEG();
    bool dashboard_open = ImGui::Begin("QuantSimX Dashboard", nullptr,
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    if (dashboard_open) {
        render_control_panel();
        ImGui::Separator();
        render_top_panel(ob, vwap_calc);
        ImGui::Separator();

        // Update active position tracking
        double current_mid = ob.get_mid_price();
        if (current_mid > 0) {
            update_active_position(current_mid, sig_gen);
        }

        DBG_BEG();
        if (ImGui::BeginTabBar("##tabs")) {
            DBG_BEG();
            if (ImGui::BeginTabItem("Live Trading")) {
                ImGui::Columns(2);
                render_orderbook_panel(ob);
                ImGui::NextColumn();
                render_price_chart_panel();
                ImGui::Columns(1);
                ImGui::EndTabItem();
                DBG_END();
            }
            DBG_BEG();
            if (ImGui::BeginTabItem("Trading Dashboard")) {
                render_trading_dashboard_panel();
                ImGui::EndTabItem();
                DBG_END();
            }
            DBG_BEG();
            if (ImGui::BeginTabItem("Regime Analysis")) {
                render_regime_analysis_panel();
                ImGui::EndTabItem();
                DBG_END();
            }
            DBG_BEG();
            if (ImGui::BeginTabItem("Signals")) {
                render_signals_panel(sig_gen);
                ImGui::EndTabItem();
                DBG_END();
            }
            DBG_BEG();
            if (ImGui::BeginTabItem("Backtest Results")) {
                render_backtest_results_panel();
                ImGui::EndTabItem();
                DBG_END();
            }
            ImGui::EndTabBar();
            DBG_END();
        }
    }
    ImGui::End();
    DBG_END();

    auto frame_end = std::chrono::high_resolution_clock::now();
    double frame_ms = std::chrono::duration<double, std::milli>(frame_end - frame_start).count();
    if (frame_ms > 16.0) {
        // print occasional warning if frame takes longer than ~60fps budget
        std::cout << "[DashboardUI] slow frame: " << frame_ms << " ms\n";
    }

    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);

    if (debug_begin_count != debug_end_count && !show_metrics_window_) {
        std::cout << "[DashboardUI] imbalanced ImGui calls: begin=" << debug_begin_count
                  << " end=" << debug_end_count << " (metrics window active?)\n";
    }

    return true;
}

void DashboardUI::render_control_panel() {
    ImGui::Text("  QuantSimX - HFT Trading Terminal");
    ImGui::SameLine(ImGui::GetWindowWidth() - 400);

    switch (app_state_) {
        case AppState::IDLE:
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
            if (ImGui::Button("CONNECT", ImVec2(120, 40))) {
                should_connect_ = true;
            }
            ImGui::PopStyleColor(2);
            break;
        case AppState::CONNECTING: {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
            connection_anim_time_ += 0.05f;
            float alpha = 0.5f + 0.5f * sin(connection_anim_time_ * 3.14f);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
            ImGui::Button("CONNECTING...", ImVec2(120, 40));
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            break;
        }
        case AppState::CONNECTED:
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
            ImGui::Text("● LIVE");
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            if (ImGui::Button("DISCONNECT", ImVec2(120, 40))) {
                should_disconnect_ = true;
            }
            ImGui::PopStyleColor(2);
            break;
        case AppState::STOPPED:
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
            ImGui::Text("● STOPPED");
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
            if (ImGui::Button("RECONNECT", ImVec2(120, 40))) {
                should_connect_ = true;
            }
            ImGui::PopStyleColor(2);
            break;
    }

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.4f, 0.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
    if (ImGui::Button("STOP", ImVec2(80, 40))) {
        should_stop_ = true;
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.6f, 1.0f, 1.0f));
    if (ImGui::Button("BACKTEST", ImVec2(100, 40))) {
        should_backtest_ = true;
    }
    ImGui::PopStyleColor(2);

    // additional debug controls (alpha sliders, metrics toggle)
    render_debug_controls();
}

void DashboardUI::render_debug_controls() {
    if (ImGui::CollapsingHeader("Debug / Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Price smoothing", &price_smooth_alpha_, 0.0f, 1.0f);
        ImGui::SliderFloat("VWAP smoothing", &vwap_smooth_alpha_, 0.0f, 1.0f);
        ImGui::Checkbox("Show ImGui metrics", &show_metrics_window_);
    }
}

void DashboardUI::render_top_panel(const OrderBook& ob, const VWAPCalculator& vwap) {
    ImGui::Columns(4);
    ImGui::SetColumnWidth(-1, 150);

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "Mid: " << ob.get_mid_price();
    ImGui::TextUnformatted(oss.str().c_str());

    oss.str("");
    oss << "Bid: " << ob.get_best_bid();
    ImGui::TextUnformatted(oss.str().c_str());

    ImGui::NextColumn();
    oss.str("");
    oss << "Ask: " << ob.get_best_ask();
    ImGui::TextUnformatted(oss.str().c_str());

    oss.str("");
    oss << "Spread: " << (ob.get_best_ask() - ob.get_best_bid());
    ImGui::TextUnformatted(oss.str().c_str());

    ImGui::NextColumn();
    oss.str("");
    oss << "VWAP: " << vwap.get_vwap();
    ImGui::TextUnformatted(oss.str().c_str());

    oss.str("");
    oss << "SMA: " << vwap.get_sma();
    ImGui::TextUnformatted(oss.str().c_str());

    ImGui::NextColumn();
    oss.str("");
    oss << std::setprecision(1) << "RSI: " << vwap.get_rsi();
    ImGui::TextUnformatted(oss.str().c_str());

    ImGui::Columns(1);
}

void DashboardUI::render_orderbook_panel(const OrderBook& ob) {
    ImGui::Text("ORDER BOOK");
    ImGui::Separator();

    ImGui::Columns(2);
    ImGui::SetColumnWidth(-1, ImGui::GetWindowWidth() * 0.5f);

    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "BIDS");
    ImGui::Separator();
    const auto* bids = ob.get_bids();
    size_t bid_count = ob.get_bid_count();
    for (size_t i = 0; i < std::min(bid_count, size_t(10)); ++i) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "$%.2f x %.4f", bids[i].price, bids[i].quantity);
    }

    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "ASKS");
    ImGui::Separator();
    const auto* asks = ob.get_asks();
    size_t ask_count = ob.get_ask_count();
    for (size_t i = 0; i < std::min(ask_count, size_t(10)); ++i) {
        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "$%.2f x %.4f", asks[i].price, asks[i].quantity);
    }

    ImGui::Columns(1);
}

void DashboardUI::render_price_chart_panel() {
    ImGui::Text("PRICE & VWAP Chart (%.0f samples)", price_history_.size());
    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Green=Price | Blue=VWAP");
    
    if (!price_history_.empty()) {
        std::vector<float> price_data(price_history_.begin(), price_history_.end());
        
        // Find min/max for Y-axis scaling
        float min_price = *std::min_element(price_data.begin(), price_data.end());
        float max_price = *std::max_element(price_data.begin(), price_data.end());
        float range = max_price - min_price;
        if (range < 0.01f) range = 0.01f;  // Avoid division by zero
        float padding = range * 0.05f;
        
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size(ImGui::GetContentRegionAvail().x, 200);
        ImVec2 canvas_max(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y);
        
        // Draw background
        draw_list->AddRectFilled(canvas_pos, canvas_max, IM_COL32(15, 15, 20, 255));
        
        // Enable clipping for lines
        draw_list->PushClipRect(canvas_pos, canvas_max, true);
        
        // Draw grid and Y-axis labels
        int num_y_lines = 5;
        for (int i = 0; i <= num_y_lines; ++i) {
            float y = canvas_pos.y + (canvas_size.y / num_y_lines) * i;
            float price_label = max_price - (range / num_y_lines) * i;
            
            // Grid line
            draw_list->AddLine(ImVec2(canvas_pos.x, y), 
                             ImVec2(canvas_max.x, y),
                             IM_COL32(50, 50, 60, 100), 0.5f);
            
            // Y-axis label
            ImGui::SetCursorScreenPos(ImVec2(canvas_pos.x - 55, y - 8));
            ImGui::Text("$%.2f", price_label);
        }
        
        // Draw price line in green
        if (price_data.size() > 1) {
            for (size_t i = 0; i < price_data.size() - 1; ++i) {
                float y1 = canvas_pos.y + canvas_size.y - ((price_data[i] - min_price + padding) / (range + 2*padding)) * canvas_size.y;
                float y2 = canvas_pos.y + canvas_size.y - ((price_data[i+1] - min_price + padding) / (range + 2*padding)) * canvas_size.y;
                float x1 = canvas_pos.x + (i / (float)price_data.size()) * canvas_size.x;
                float x2 = canvas_pos.x + ((i+1) / (float)price_data.size()) * canvas_size.x;
                
                ImVec2 p1(x1, y1), p2(x2, y2);
                // Clamp to canvas boundaries
                p1.y = std::max(canvas_pos.y, std::min(canvas_max.y, p1.y));
                p2.y = std::max(canvas_pos.y, std::min(canvas_max.y, p2.y));
                draw_list->AddLine(p1, p2, IM_COL32(50, 200, 100, 255), 1.5f);
            }
        }
        
        // Draw VWAP line in blue if available
        if (!vwap_history_.empty()) {
            std::vector<float> vwap_data(vwap_history_.begin(), vwap_history_.end());
            if (vwap_data.size() > 1) {
                for (size_t i = 0; i < vwap_data.size() - 1; ++i) {
                    float y1 = canvas_pos.y + canvas_size.y - ((vwap_data[i] - min_price + padding) / (range + 2*padding)) * canvas_size.y;
                    float y2 = canvas_pos.y + canvas_size.y - ((vwap_data[i+1] - min_price + padding) / (range + 2*padding)) * canvas_size.y;
                    float x1 = canvas_pos.x + (i / (float)vwap_data.size()) * canvas_size.x;
                    float x2 = canvas_pos.x + ((i+1) / (float)vwap_data.size()) * canvas_size.x;
                    
                    ImVec2 p1(x1, y1), p2(x2, y2);
                    // Clamp to canvas boundaries
                    p1.y = std::max(canvas_pos.y, std::min(canvas_max.y, p1.y));
                    p2.y = std::max(canvas_pos.y, std::min(canvas_max.y, p2.y));
                    draw_list->AddLine(p1, p2, IM_COL32(100, 150, 255, 255), 1.5f);
                }
            }
        }
        
        draw_list->PopClipRect();
        
        // Draw border
        draw_list->AddRect(canvas_pos, canvas_max, IM_COL32(100, 100, 120, 255));
        
        ImGui::Dummy(canvas_size);
    }
}

// Regime analysis panel replaced old OHLC chart
void DashboardUI::render_regime_analysis_panel() {
    ImGui::Text("REGIME ANALYSIS");
    ImGui::Separator();
    ImGui::Text("Low-vol vs High-vol regime probabilities and forecast volatility");
    
    compute_regime_analysis();
    
    if (!analysis_volatility_.empty()) {
        ImGui::Text("Forecast Volatility (green) and Returns (yellow)");
        // convert to float arrays
        std::vector<float> volf(analysis_volatility_.begin(), analysis_volatility_.end());
        std::vector<float> retf(analysis_returns_.begin(), analysis_returns_.end());
        float vmax = *std::max_element(volf.begin(), volf.end());
        float rmin = *std::min_element(retf.begin(), retf.end());
        float rmax = *std::max_element(retf.begin(), retf.end());
        ImGui::PlotLines("##vol", volf.data(), (int)volf.size(), 0, nullptr, 0.0f, vmax, ImVec2(0,100));
        ImGui::PlotLines("##ret", retf.data(), (int)retf.size(), 0, nullptr, rmin, rmax, ImVec2(0,100));
    }
    if (!filtered_prob_low_.empty()) {
        ImGui::Text("Low-vol regime probability");
        std::vector<float> lp(filtered_prob_low_.begin(), filtered_prob_low_.end());
        std::vector<float> hp(filtered_prob_high_.begin(), filtered_prob_high_.end());
        ImGui::PlotLines("##prob_lo", lp.data(), (int)lp.size(), 0, nullptr, 0.0f, 1.0f, ImVec2(0,100));
        ImGui::Text("High-vol regime probability");
        ImGui::PlotLines("##prob_hi", hp.data(), (int)hp.size(), 0, nullptr, 0.0f, 1.0f, ImVec2(0,100));
    }
}

void DashboardUI::compute_regime_analysis() {
    // compute returns from smoothed price history
    analysis_returns_.clear();
    analysis_volatility_.clear();
    filtered_prob_low_.clear();
    filtered_prob_high_.clear();

    if (price_history_.size() < 2) return;
    
    // calculate log returns
    for (size_t i = 1; i < price_history_.size(); ++i) {
        double r = log(price_history_[i] / price_history_[i-1]);
        analysis_returns_.push_back(r);
    }

    // simple GARCH(1,1) volatility forecast on returns
    double omega = 0.000001;
    double alpha = 0.1;
    double beta = 0.85;
    double prev_vol = 0.0;
    for (size_t i = 0; i < analysis_returns_.size(); ++i) {
        double eps2 = analysis_returns_[i] * analysis_returns_[i];
        double vol = omega + alpha * eps2 + beta * prev_vol;
        analysis_volatility_.push_back(sqrt(vol));
        prev_vol = vol;
    }

    // compute regime probabilities using logistic on volatility
    // low-vol probability = 1 / (1 + exp((vol - median)*10))
    if (!analysis_volatility_.empty()) {
        std::vector<double> sorted = analysis_volatility_;
        std::sort(sorted.begin(), sorted.end());
        double median = sorted[sorted.size()/2];
        for (double v : analysis_volatility_) {
            double p_low = 1.0 / (1.0 + exp((v - median) * 10.0));
            filtered_prob_low_.push_back(p_low);
            filtered_prob_high_.push_back(1.0 - p_low);
        }
    }
}

void DashboardUI::render_stats_panel(const VWAPCalculator& vwap) {
    ImGui::Text("STATISTICS");
    ImGui::Separator();
    ImGui::Text("VWAP: %.2f", vwap.get_vwap());
    ImGui::Text("SMA(20): %.2f", vwap.get_sma());
    ImGui::Text("RSI(14): %.1f", vwap.get_rsi());
}

void DashboardUI::render_signals_panel(const SignalGenerator& sig_gen) {
    ImGui::Text("TRADING SIGNALS");
    ImGui::Separator();

    const auto& signals = sig_gen.get_signals();
    for (size_t i = 0; i < std::min(signals.size(), size_t(20)); ++i) {
        auto sig = signals[signals.size() - 1 - i];
        std::string sig_str = sig.signal == Signal::BUY ? "BUY" : 
                             sig.signal == Signal::SELL ? "SELL" : "NONE";
        ImGui::Text("[%s] Price: %.2f VWAP: %.2f Reason: %s",
                   sig_str.c_str(), sig.price, sig.vwap, sig.reason.c_str());
    }
}

void DashboardUI::render_backtest_results_panel() {
    ImGui::Text("BACKTEST RESULTS");
    ImGui::Separator();

    if (backtest_has_results_) {
        ImGui::Text("Total Trades: %d", backtest_metrics_.total_trades);
        ImGui::Text("Win Rate: %.1f%%", backtest_metrics_.win_rate * 100.0);
        ImGui::Text("Total P&L: %.2f", backtest_metrics_.total_pnl);
        ImGui::Text("Profit Factor: %.2f", backtest_metrics_.profit_factor);
        ImGui::Text("Max Drawdown: %.2f%%", backtest_metrics_.max_drawdown * 100.0);
        ImGui::Text("Sharpe Ratio: %.2f", backtest_metrics_.sharpe_ratio);
    } else {
        ImGui::Text("No backtest results available");
    }
}

void DashboardUI::render_trading_dashboard_panel() {
    ImGui::Text("LIVE TRADING DASHBOARD");
    ImGui::Separator();

    // P&L and metrics header
    ImGui::Columns(4);
    ImGui::SetColumnWidth(-1, 150);
    
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
    ImGui::Text("Cumulative P&L: $%.2f", cumulative_pnl_);
    ImGui::PopStyleColor();

    ImGui::NextColumn();
    int total_trades_display = total_trades_;
    ImGui::Text("Total Trades: %d", total_trades_display);

    ImGui::NextColumn();
    double win_rate = total_trades_display > 0 ? (double)winning_trades_ / total_trades_display * 100.0 : 0.0;
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
    ImGui::Text("Win Rate: %.1f%%", win_rate);
    ImGui::PopStyleColor();

    ImGui::NextColumn();
    ImGui::Text("Winning: %d | Losing: %d", winning_trades_, losing_trades_);

    ImGui::Columns(1);
    ImGui::Separator();

    // Active position section
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "ACTIVE POSITION");
    
    if (has_active_position_) {
        ImGui::Columns(2);
        ImGui::SetColumnWidth(-1, 250);
        
        // Position details (left column)
        ImGui::Text("Signal Type:");
        ImGui::SameLine(100);
        ImGui::TextColored(
            active_position_.signal_type == "BUY" ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(1.0f, 0.2f, 0.2f, 1.0f),
            "%s", active_position_.signal_type.c_str()
        );

        ImGui::Text("Entry Price:");
        ImGui::SameLine(100);
        ImGui::Text("$%.2f", active_position_.entry_price);

        ImGui::Text("Current Price:");
        ImGui::SameLine(100);
        ImGui::Text("$%.2f", active_position_.current_price);

        ImGui::Text("Quantity:");
        ImGui::SameLine(100);
        ImGui::Text("%.4f", active_position_.quantity);

        // Time elapsed
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch() - std::chrono::milliseconds(active_position_.entry_time)
        );
        ImGui::Text("Duration:");
        ImGui::SameLine(100);
        ImGui::Text("%ld seconds", duration.count());

        ImGui::NextColumn();
        
        // P&L display (right column)
        ImGui::Text("Position P&L:");
        ImGui::SameLine(150);
        ImGui::PushStyleColor(ImGuiCol_Text, 
            active_position_.pnl >= 0 ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(1.0f, 0.2f, 0.2f, 1.0f)
        );
        ImGui::Text("$%.2f", active_position_.pnl);
        ImGui::PopStyleColor();

        ImGui::Text("P&L %:");
        ImGui::SameLine(150);
        ImGui::PushStyleColor(ImGuiCol_Text, 
            active_position_.pnl_percent >= 0 ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(1.0f, 0.2f, 0.2f, 1.0f)
        );
        ImGui::Text("%.2f%%", active_position_.pnl_percent);
        ImGui::PopStyleColor();

        ImGui::Columns(1);
    } else {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "No active position");
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "TRADE HISTORY (Last 20)");
    ImGui::Separator();

    if (!closed_positions_.empty()) {
        ImGui::Columns(5);
        ImGui::SetColumnWidth(-1, 80);
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 1.0f, 1.0f), "Type");
        ImGui::NextColumn();
        ImGui::SetColumnWidth(-1, 100);
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 1.0f, 1.0f), "Entry Price");
        ImGui::NextColumn();
        ImGui::SetColumnWidth(-1, 100);
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 1.0f, 1.0f), "Exit Price");
        ImGui::NextColumn();
        ImGui::SetColumnWidth(-1, 80);
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 1.0f, 1.0f), "P&L $");
        ImGui::NextColumn();
        ImGui::SetColumnWidth(-1, 80);
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 1.0f, 1.0f), "P&L %");
        ImGui::NextColumn();
        ImGui::Separator();

        // Display last 20 trades in reverse order (most recent first)
        int start_idx = std::max(0, (int)closed_positions_.size() - 20);
        for (int i = (int)closed_positions_.size() - 1; i >= start_idx; --i) {
            const auto& pos = closed_positions_[i];
            
            ImGui::TextColored(
                pos.signal_type == "BUY" ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(1.0f, 0.2f, 0.2f, 1.0f),
                "%s", pos.signal_type.c_str()
            );
            ImGui::NextColumn();

            ImGui::Text("$%.2f", pos.entry_price);
            ImGui::NextColumn();

            ImGui::Text("$%.2f", pos.current_price);
            ImGui::NextColumn();

            ImGui::PushStyleColor(ImGuiCol_Text, 
                pos.pnl >= 0 ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(1.0f, 0.2f, 0.2f, 1.0f)
            );
            ImGui::Text("$%.2f", pos.pnl);
            ImGui::PopStyleColor();
            ImGui::NextColumn();

            ImGui::PushStyleColor(ImGuiCol_Text, 
                pos.pnl_percent >= 0 ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(1.0f, 0.2f, 0.2f, 1.0f)
            );
            ImGui::Text("%.2f%%", pos.pnl_percent);
            ImGui::PopStyleColor();
            ImGui::NextColumn();
        }
        ImGui::Columns(1);
    } else {
        ImGui::Text("No closed trades yet");
    }
}

void DashboardUI::update_active_position(double current_price, const SignalGenerator& sig_gen) {
    // Get the latest signal to open/close positions
    const auto& signals = sig_gen.get_signals();
    
    if (signals.empty()) return;

    SignalEvent latest_signal = signals.back();
    
    // If we don't have an active position and we get a BUY/SELL signal, open one
    if (!has_active_position_ && latest_signal.signal != Signal::NONE) {
        has_active_position_ = true;
        active_position_.entry_time = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        active_position_.entry_price = latest_signal.price;
        active_position_.current_price = current_price;
        active_position_.quantity = 1.0;
        active_position_.signal_type = (latest_signal.signal == Signal::BUY) ? "BUY" : "SELL";
        total_trades_++;
    }

    // Update current price for active position
    if (has_active_position_) {
        active_position_.current_price = current_price;
        
        // Calculate P&L
        if (active_position_.signal_type == "BUY") {
            active_position_.pnl = (current_price - active_position_.entry_price) * active_position_.quantity;
            active_position_.pnl_percent = ((current_price - active_position_.entry_price) / active_position_.entry_price) * 100.0;
        } else {
            active_position_.pnl = (active_position_.entry_price - current_price) * active_position_.quantity;
            active_position_.pnl_percent = ((active_position_.entry_price - current_price) / active_position_.entry_price) * 100.0;
        }

        // Check if we should close the position (opposite signal)
        if ((active_position_.signal_type == "BUY" && latest_signal.signal == Signal::SELL) ||
            (active_position_.signal_type == "SELL" && latest_signal.signal == Signal::BUY)) {
            
            // Close the position
            cumulative_pnl_ += active_position_.pnl;
            
            // Track closed position in history
            ActivePosition closed = active_position_;
            closed.current_price = current_price;
            closed_positions_.push_back(closed);
            if (closed_positions_.size() > 100) {
                closed_positions_.pop_front();
            }

            // Update win/loss count
            if (active_position_.pnl > 0.0) {
                winning_trades_++;
            } else if (active_position_.pnl < 0.0) {
                losing_trades_++;
            }
            // Note: breakeven trades (pnl == 0) are counted in total but not in wins/losses

            // Clear active position
            has_active_position_ = false;
        }
    }
}

void DashboardUI::reset_button_flags() {
    should_connect_ = false;
    should_disconnect_ = false;
    should_stop_ = false;
    should_backtest_ = false;
}

void DashboardUI::set_app_state(AppState state) {
    app_state_ = state;
    if (state == AppState::CONNECTING) {
        connection_anim_time_ = 0.0f;
    }
}
