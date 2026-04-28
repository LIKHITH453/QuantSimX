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

namespace ImGui {

void StyleColorsTerminal() {
    ImGuiStyle* style = &ImGui::GetStyle();
    ImVec4 bg(0.02f, 0.02f, 0.04f, 1.0f);
    ImVec4 panel(0.06f, 0.06f, 0.10f, 1.0f);
    ImVec4 border(0.15f, 0.15f, 0.25f, 1.0f);
    ImVec4 text(0.85f, 0.85f, 0.90f, 1.0f);
    ImVec4 text_dim(0.45f, 0.45f, 0.55f, 1.0f);
    ImVec4 accent(0.0f, 0.8f, 0.9f, 1.0f);
    ImVec4 green(0.1f, 0.9f, 0.4f, 1.0f);
    ImVec4 red(0.95f, 0.2f, 0.2f, 1.0f);
    ImVec4 yellow(0.95f, 0.85f, 0.1f, 1.0f);
    ImVec4 blue(0.2f, 0.5f, 1.0f, 1.0f);

    style->Colors[ImGuiCol_WindowBg] = bg;
    style->Colors[ImGuiCol_ChildBg] = panel;
    style->Colors[ImGuiCol_Border] = border;
    style->Colors[ImGuiCol_Text] = text;
    style->Colors[ImGuiCol_TextDisabled] = text_dim;
    style->Colors[ImGuiCol_Header] = panel;
    style->Colors[ImGuiCol_HeaderHovered] = ImVec4(0.1f, 0.1f, 0.15f, 1.0f);
    style->Colors[ImGuiCol_Button] = ImVec4(0.1f, 0.3f, 0.4f, 1.0f);
    style->Colors[ImGuiCol_ButtonHovered] = ImVec4(0.15f, 0.4f, 0.5f, 1.0f);
    style->Colors[ImGuiCol_ButtonActive] = accent;
    style->Colors[ImGuiCol_Tab] = panel;
    style->Colors[ImGuiCol_TabHovered] = accent;
    style->Colors[ImGuiCol_TabActive] = ImVec4(0.0f, 0.6f, 0.7f, 1.0f);
    style->Colors[ImGuiCol_TableHeaderBg] = panel;
    style->Colors[ImGuiCol_TableRowBg] = ImVec4(0.06f, 0.06f, 0.10f, 1.0f);
    style->Colors[ImGuiCol_PlotLines] = accent;
    style->Colors[ImGuiCol_PlotHistogram] = blue;

    style->WindowBorderSize = 1.0f;
    style->WindowRounding = 4.0f;
    style->FrameBorderSize = 1.0f;
    style->FrameRounding = 3.0f;
    style->ItemSpacing = ImVec2(8, 4);
    style->CellPadding = ImVec2(6, 4);
}

bool BeginTable(const char* id, int columns) {
    return ImGui::BeginTable(id, columns, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoClip);
}

void TableRow(const char* col0, const char* col1, const char* col2, const char* col3) {
    ImGui::TableNextColumn(); ImGui::TextUnformatted(col0);
    ImGui::TableNextColumn(); ImGui::TextUnformatted(col1);
    if (col2) { ImGui::TableNextColumn(); ImGui::TextUnformatted(col2); }
    if (col3) { ImGui::TableNextColumn(); ImGui::TextUnformatted(col3); }
}

void NextColumn() { ImGui::TableNextColumn(); }

void EndTable() { ImGui::EndTable(); }

void SeparatorV() { ImGui::Separator(); }

void TextHighlight(const char* text, ImVec4 color) {
    ImGui::TextColored(color, "%s", text);
}

void PanelHeader(const char* title) {
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
    ImGui::TextColored(ImVec4(0.0f, 0.75f, 0.85f, 1.0f), "%s", title);
    ImGui::Separator();
}

}

static const ImVec4 COL_GREEN(0.1f, 0.9f, 0.4f, 1.0f);
static const ImVec4 COL_RED(0.95f, 0.2f, 0.2f, 1.0f);
static const ImVec4 COL_YELLOW(0.95f, 0.85f, 0.1f, 1.0f);
static const ImVec4 COL_CYAN(0.0f, 0.8f, 0.9f, 1.0f);
static const ImVec4 COL_BLUE(0.2f, 0.5f, 1.0f, 1.0f);
static const ImVec4 COL_DIM(0.45f, 0.45f, 0.55f, 1.0f);

bool DashboardUI::init(int width, int height) {
    if (!glfwInit()) return false;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_FALSE);

    window_ = glfwCreateWindow(width, height, "QuantSimX", nullptr, nullptr);
    if (!window_) { glfwTerminate(); return false; }

    glfwMakeContextCurrent((GLFWwindow*)window_);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsTerminal();
    ImGui_ImplGlfw_InitForOpenGL((GLFWwindow*)window_, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    return true;
}

void DashboardUI::shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if (window_) {
        glfwDestroyWindow((GLFWwindow*)window_);
        window_ = nullptr;
    }
    glfwTerminate();
}

bool DashboardUI::render(const OrderBook& ob, const VWAPCalculator& vwap_calc, const SignalGenerator& sig_gen) {
    GLFWwindow* win = (GLFWwindow*)window_;
    auto frame_start = std::chrono::high_resolution_clock::now();

    if (glfwWindowShouldClose(win)) {
        close_requested_ = true;
        return false;
    }

    double mid = ob.get_mid_price();
    if (mid > 0) {
        if (displayed_price_ <= 0) displayed_price_ = mid;
        else displayed_price_ += (mid - displayed_price_) * smooth_alpha_;
        price_history_.push_back(displayed_price_);
        if (price_history_.size() > PRICE_HISTORY_MAX) price_history_.pop_front();

        double vwap = vwap_calc.get_vwap();
        if (vwap > 0) {
            if (displayed_vwap_ <= 0) displayed_vwap_ = vwap;
            else displayed_vwap_ += (vwap - displayed_vwap_) * smooth_alpha_;
            vwap_history_.push_back(displayed_vwap_);
            if (vwap_history_.size() > PRICE_HISTORY_MAX) vwap_history_.pop_front();
        }

        double rsi = vwap_calc.get_rsi();
        rsi_history_.push_back(rsi);
        if (rsi_history_.size() > PRICE_HISTORY_MAX) rsi_history_.pop_front();
    }

    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

    ImGui::Begin("QuantSimX", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBringToFrontOnFocus);

    render_header();
    ImGui::Separator();

    render_toolbar();
    ImGui::Separator();

    if (ImGui::BeginTabBar("##main_tabs", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("Market Watch")) {
            ImGui::Columns(2, "market_cols", true);
            render_market_watch_panel(ob, vwap_calc);
            ImGui::NextColumn();
            render_chart_panel();
            ImGui::Columns(1);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Order Book")) {
            render_orderbook_panel(ob);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Trades")) {
            render_trades_panel();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Performance")) {
            render_performance_panel();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Signals")) {
            render_signals_panel(sig_gen);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Backtest")) {
            render_backtest_panel();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();

    auto frame_end = std::chrono::high_resolution_clock::now();
    double frame_ms = std::chrono::duration<double, std::milli>(frame_end - frame_start).count();
    if (frame_ms > 16.0) std::cout << "[UI] slow frame: " << frame_ms << "ms\n";

    ImGui::Render();
    int w, h;
    glfwGetFramebufferSize(win, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0.02f, 0.02f, 0.04f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(win);

    return true;
}

void DashboardUI::render_header() {
    ImGui::Columns(4, "header_cols", false);
    ImGui::SetColumnWidth(-1, 200);

    ImGui::TextColored(COL_CYAN, "QUANTSIMX");
    ImGui::TextColored(COL_DIM, "BTCUSDT Market Data");
    ImGui::NextColumn();

    double bid = 0, ask = 0;
    if (!price_history_.empty()) {
        bid = displayed_price_ - 0.01;
        ask = displayed_price_ + 0.01;
    }
    ImGui::TextColored(COL_GREEN, "BID  $%.2f", bid);
    ImGui::TextColored(COL_RED, "ASK  $%.2f", ask);
    ImGui::TextColored(COL_DIM, "SPREAD %.2f", ask - bid);
    ImGui::NextColumn();

    ImGui::TextColored(COL_YELLOW, "VWAP $%.2f", displayed_vwap_);
    ImGui::TextColored(COL_DIM, "SMA   $%.2f", displayed_vwap_);
    ImGui::NextColumn();

    if (!rsi_history_.empty()) {
        double rsi = rsi_history_.back();
        ImVec4 rsi_col = (rsi < 30) ? COL_GREEN : (rsi > 70) ? COL_RED : COL_DIM;
        ImGui::TextColored(rsi_col, "RSI   %.1f", rsi);
    }
    ImGui::TextColored(COL_DIM, "TRADES %zu", price_history_.size());

    ImGui::Columns(1);
}

void DashboardUI::render_toolbar() {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.5f, 0.4f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.7f, 0.5f, 1.0f));
    if (ImGui::Button("CONNECT", ImVec2(100, 32))) connect_requested_ = true;
    ImGui::PopStyleColor(2);

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.15f, 0.15f, 1.0f));
    if (ImGui::Button("DISCONNECT", ImVec2(100, 32))) disconnect_requested_ = true;
    ImGui::PopStyleColor(2);

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.35f, 0.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.5f, 0.0f, 1.0f));
    if (ImGui::Button("STOP", ImVec2(80, 32))) stop_requested_ = true;
    ImGui::PopStyleColor(2);

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.3f, 0.6f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.4f, 0.8f, 1.0f));
    if (ImGui::Button("BACKTEST", ImVec2(100, 32))) backtest_requested_ = true;
    ImGui::PopStyleColor(2);

    ImGui::SameLine(ImGui::GetWindowWidth() - 220);
    ImGui::TextColored(COL_DIM, "Smooth:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    ImGui::SliderFloat("##smooth", &smooth_alpha_, 0.01f, 0.5f, "%.2f");

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.2f, 1.0f));
    if (ImGui::Button("SETTINGS", ImVec2(80, 32))) settings_open_ = !settings_open_;
    ImGui::PopStyleColor();

    if (settings_open_) {
        ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - 320, 80));
        ImGui::BeginGroup();
        ImGui::SetNextItemWidth(150);
        ImGui::SliderFloat("Smoothing", &smooth_alpha_, 0.01f, 0.5f, "%.2f");
        ImGui::EndGroup();
    }
}

void DashboardUI::render_market_watch_panel(const OrderBook& ob, const VWAPCalculator& vwap) {
    ImGui::PanelHeader("MARKET WATCH");

    if (ImGui::BeginTable("##market", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextColored(COL_DIM, "LAST PRICE");
        ImGui::TableNextColumn(); ImGui::TextColored(COL_CYAN, "$%.2f", displayed_price_);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextColored(COL_DIM, "BEST BID");
        ImGui::TableNextColumn(); ImGui::TextColored(COL_GREEN, "$%.2f", ob.get_best_bid());

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextColored(COL_DIM, "BEST ASK");
        ImGui::TableNextColumn(); ImGui::TextColored(COL_RED, "$%.2f", ob.get_best_ask());

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextColored(COL_DIM, "MID PRICE");
        ImGui::TableNextColumn(); ImGui::TextColored(COL_CYAN, "$%.2f", ob.get_mid_price());

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextColored(COL_DIM, "SPREAD");
        ImGui::TableNextColumn(); ImGui::Text("$%.2f", ob.get_best_ask() - ob.get_best_bid());

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextColored(COL_DIM, "VWAP");
        ImGui::TableNextColumn(); ImGui::TextColored(COL_YELLOW, "$%.2f", vwap.get_vwap());

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextColored(COL_DIM, "RSI (14)");
        double rsi_val = vwap.get_rsi();
        ImVec4 rsi_col = (rsi_val < 30) ? COL_GREEN : (rsi_val > 70) ? COL_RED : COL_CYAN;
        ImGui::TableNextColumn(); ImGui::TextColored(rsi_col, "%.1f", rsi_val);

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::PanelHeader("ACCOUNT");
    if (ImGui::BeginTable("##account", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextColored(COL_DIM, "EQUITY");
        ImGui::TableNextColumn(); ImGui::TextColored(COL_CYAN, "$%.2f", account_.equity);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextColored(COL_DIM, "REALIZED P&L");
        ImVec4 pnl_col = (account_.realized_pnl >= 0) ? COL_GREEN : COL_RED;
        ImGui::TableNextColumn(); ImGui::TextColored(pnl_col, "$%.2f", account_.realized_pnl);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextColored(COL_DIM, "UNREALIZED P&L");
        ImVec4 upnl_col = (account_.unrealized_pnl >= 0) ? COL_GREEN : COL_RED;
        ImGui::TableNextColumn(); ImGui::TextColored(upnl_col, "$%.2f", account_.unrealized_pnl);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextColored(COL_DIM, "DRAWDOWN");
        ImGui::TableNextColumn(); ImGui::TextColored(COL_RED, "%.2f%%", account_.max_drawdown * 100.0);

        ImGui::EndTable();
    }
}

void DashboardUI::render_orderbook_panel(const OrderBook& ob) {
    ImGui::PanelHeader("ORDER BOOK - BTCUSDT");

    const auto* bids = ob.get_bids();
    const auto* asks = ob.get_asks();
    size_t bid_cnt = ob.get_bid_count();
    size_t ask_cnt = ob.get_ask_count();

    double max_qty = 0.0001;
    for (size_t i = 0; i < std::min(bid_cnt, size_t(15)); ++i)
        max_qty = std::max(max_qty, bids[i].quantity);
    for (size_t i = 0; i < std::min(ask_cnt, size_t(15)); ++i)
        max_qty = std::max(max_qty, asks[i].quantity);

    if (ImGui::BeginTable("##ob", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_NoClip)) {
        ImGui::TableSetupColumn("BID QTY", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("PRICE", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("ASK QTY", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableHeadersRow();

        size_t levels = std::min(std::max(bid_cnt, ask_cnt), size_t(15));
        for (size_t i = 0; i < levels; ++i) {
            ImGui::TableNextRow();

            if (i < bid_cnt) {
                float bar_w = (bids[i].quantity / max_qty) * 80.0f;
                ImGui::TableNextColumn();
                ImGui::TextColored(COL_GREEN, "%.4f", bids[i].quantity);
                ImGui::SameLine(70);
                ImGui::GetWindowDrawList()->AddRectFilled(
                    ImGui::GetCursorScreenPos(),
                    ImVec2(ImGui::GetCursorScreenPos().x + bar_w, ImGui::GetCursorScreenPos().y + 12),
                    IM_COL32(20, 100, 50, 150));

                ImGui::TableNextColumn();
                ImGui::TextColored(COL_GREEN, "$%.2f", bids[i].price);
            } else {
                ImGui::TableNextColumn();
                ImGui::TableNextColumn();
            }

            if (i < ask_cnt) {
                ImGui::TableNextColumn();
                ImGui::TextColored(COL_RED, "$%.2f", asks[i].price);
                ImGui::SameLine(180);
                ImGui::TextColored(COL_RED, "%.4f", asks[i].quantity);
            }
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    double spread = ob.get_best_ask() - ob.get_best_bid();
    double spread_bps = (ob.get_best_bid() > 0) ? (spread / ob.get_best_bid()) * 10000 : 0;
    ImGui::TextColored(COL_YELLOW, "SPREAD: $%.2f  (%.1f bps)", spread, spread_bps);
}

void DashboardUI::render_chart_panel() {
    ImGui::PanelHeader("PRICE CHART");

    if (price_history_.empty()) {
        ImGui::TextColored(COL_DIM, "Waiting for market data...");
        return;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 canvas = ImGui::GetCursorScreenPos();
    ImVec2 size(ImGui::GetContentRegionAvail().x, 280);
    ImVec2 canvas_max(canvas.x + size.x, canvas.y + size.y);

    dl->AddRectFilled(canvas, canvas_max, IM_COL32(8, 8, 15, 255));

    float min_p = *std::min_element(price_history_.begin(), price_history_.end());
    float max_p = *std::max_element(price_history_.begin(), price_history_.end());
    float range = std::max(max_p - min_p, 0.01f);
    float pad = range * 0.1f;

    dl->PushClipRect(canvas, canvas_max, true);

    int grid = 5;
    for (int i = 0; i <= grid; ++i) {
        float y = canvas.y + (size.y / grid) * i;
        float label = max_p - (range / grid) * i;
        dl->AddLine(ImVec2(canvas.x, y), ImVec2(canvas_max.x, y), IM_COL32(30, 30, 45, 200), 0.5f);
        ImGui::SetCursorScreenPos(ImVec2(canvas.x - 60, y - 8));
        ImGui::TextColored(COL_DIM, "$%.2f", label);
    }

    if (price_history_.size() > 1) {
        float step = size.x / (price_history_.size() - 1);
        for (size_t i = 0; i < price_history_.size() - 1; ++i) {
            float y1 = canvas.y + size.y - ((price_history_[i] - min_p + pad) / (range + 2*pad)) * size.y;
            float y2 = canvas.y + size.y - ((price_history_[i+1] - min_p + pad) / (range + 2*pad)) * size.y;
            float x1 = canvas.x + i * step;
            float x2 = canvas.x + (i+1) * step;
            y1 = std::clamp(y1, canvas.y + 2, canvas_max.y - 2);
            y2 = std::clamp(y2, canvas.y + 2, canvas_max.y - 2);

            ImU32 col = (price_history_[i+1] >= price_history_[i]) ?
                       IM_COL32(0, 255, 130, 255) : IM_COL32(255, 60, 60, 255);
            dl->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), col, 1.5f);
        }

        if (!vwap_history_.empty()) {
            for (size_t i = 0; i < vwap_history_.size() - 1 && i < price_history_.size() - 1; ++i) {
                float y1 = canvas.y + size.y - ((vwap_history_[i] - min_p + pad) / (range + 2*pad)) * size.y;
                float y2 = canvas.y + size.y - ((vwap_history_[i+1] - min_p + pad) / (range + 2*pad)) * size.y;
                float x1 = canvas.x + i * step;
                float x2 = canvas.x + (i+1) * step;
                y1 = std::clamp(y1, canvas.y + 2, canvas_max.y - 2);
                y2 = std::clamp(y2, canvas.y + 2, canvas_max.y - 2);
                dl->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(255, 200, 50, 200), 1.2f);
            }
        }
    }

    dl->PopClipRect();
    dl->AddRect(canvas, canvas_max, IM_COL32(50, 50, 70, 255));
    ImGui::Dummy(size);

    ImGui::TextColored(COL_GREEN, "  Price (green/red)");
    ImGui::SameLine(150);
    ImGui::TextColored(COL_YELLOW, "VWAP (yellow)");
}

void DashboardUI::render_trades_panel() {
    ImGui::PanelHeader("TRADE HISTORY");

    if (closed_trades_.empty()) {
        ImGui::TextColored(COL_DIM, "No closed trades yet");
        return;
    }

    if (ImGui::BeginTable("##trades", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("TYPE", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("ENTRY", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("EXIT", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("P&L", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("P&L %", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableHeadersRow();

        int start = std::max(0, (int)closed_trades_.size() - TRADES_DISPLAY_MAX);
        for (int i = (int)closed_trades_.size() - 1; i >= start; --i) {
            const auto& t = closed_trades_[i];
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::TextColored(t.is_long ? COL_GREEN : COL_RED, t.is_long ? "LONG" : "SHORT");

            ImGui::TableNextColumn();
            ImGui::Text("$%.2f", t.entry_price);

            ImGui::TableNextColumn();
            ImGui::Text("$%.2f", t.exit_price);

            ImGui::TableNextColumn();
            ImGui::TextColored(t.pnl >= 0 ? COL_GREEN : COL_RED, "$%.2f", t.pnl);

            ImGui::TableNextColumn();
            ImGui::TextColored(t.pnl_percent >= 0 ? COL_GREEN : COL_RED, "%.2f%%", t.pnl_percent);
        }
        ImGui::EndTable();
    }
}

void DashboardUI::render_performance_panel() {
    ImGui::PanelHeader("PERFORMANCE METRICS");

    if (has_backtest_results_) {
        if (ImGui::BeginTable("##perf", 2, ImGuiTableFlags_Borders)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored(COL_DIM, "Total Trades");
            ImGui::TableNextColumn(); ImGui::Text("%d", backtest_results_.total_trades);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored(COL_DIM, "Win Rate");
            ImGui::TableNextColumn(); ImGui::TextColored(COL_CYAN, "%.1f%%", backtest_results_.win_rate * 100.0);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored(COL_DIM, "Total P&L");
            ImGui::TableNextColumn(); ImGui::TextColored(backtest_results_.total_pnl >= 0 ? COL_GREEN : COL_RED,
                          "$%.2f", backtest_results_.total_pnl);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored(COL_DIM, "Profit Factor");
            ImGui::TableNextColumn(); ImGui::Text("%.2f", backtest_results_.profit_factor);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored(COL_DIM, "Max Drawdown");
            ImGui::TableNextColumn(); ImGui::TextColored(COL_RED, "%.2f%%", backtest_results_.max_drawdown * 100.0);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored(COL_DIM, "Sharpe Ratio");
            ImGui::TableNextColumn(); ImGui::Text("%.2f", backtest_results_.sharpe_ratio);

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::PanelHeader("EQUITY CURVE");
        draw_equity_curve();

        ImGui::Spacing();
        ImGui::PanelHeader("P&L DISTRIBUTION");
        draw_pnl_histogram();
    } else {
        ImGui::TextColored(COL_DIM, "Run backtest to see performance metrics");
    }
}

void DashboardUI::draw_equity_curve() {
    if (closed_trades_.empty()) return;

    std::vector<float> equity;
    float running = 0;
    for (const auto& t : closed_trades_) {
        running += t.pnl;
        equity.push_back(running);
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 canvas = ImGui::GetCursorScreenPos();
    ImVec2 size(ImGui::GetContentRegionAvail().x, 120);
    ImVec2 end(canvas.x + size.x, canvas.y + size.y);

    dl->AddRectFilled(canvas, end, IM_COL32(8, 8, 15, 255));
    dl->PushClipRect(canvas, end, true);

    float min_eq = *std::min_element(equity.begin(), equity.end());
    float max_eq = *std::max_element(equity.begin(), equity.end());
    float eq_range = std::max(max_eq - min_eq, 1.0f);
    float step = size.x / (equity.size() - 1);

    for (size_t i = 0; i < equity.size() - 1; ++i) {
        float y1 = canvas.y + size.y - ((equity[i] - min_eq) / eq_range) * size.y;
        float y2 = canvas.y + size.y - ((equity[i+1] - min_eq) / eq_range) * size.y;
        float x1 = canvas.x + i * step;
        float x2 = canvas.x + (i+1) * step;
        ImU32 col = equity[i+1] >= equity[i] ? IM_COL32(0, 200, 100, 255) : IM_COL32(200, 50, 50, 255);
        dl->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), col, 1.5f);
    }

    dl->PopClipRect();
    dl->AddRect(canvas, end, IM_COL32(50, 50, 70, 255));
    ImGui::Dummy(size);
}

void DashboardUI::draw_pnl_histogram() {
    if (closed_trades_.empty()) return;

    std::vector<float> pnls;
    for (const auto& t : closed_trades_) pnls.push_back(t.pnl_percent);

    float min_pnl = *std::min_element(pnls.begin(), pnls.end());
    float max_pnl = *std::max_element(pnls.begin(), pnls.end());
    ImGui::PlotHistogram("##pnl", pnls.data(), (int)pnls.size(), 0, nullptr, min_pnl, max_pnl, ImVec2(0, 100));
}

void DashboardUI::render_signals_panel(const SignalGenerator& sig_gen) {
    ImGui::PanelHeader("TRADING SIGNALS");

    const auto& signals = sig_gen.get_signals();
    if (signals.empty()) {
        ImGui::TextColored(COL_DIM, "No signals generated");
        return;
    }

    if (ImGui::BeginTable("##sigs", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("TIME", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("SIGNAL", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("PRICE", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("REASON", ImGuiTableColumnFlags_WidthFixed, 200);
        ImGui::TableHeadersRow();

        size_t start = (signals.size() > 50) ? signals.size() - 50 : 0;
        for (size_t i = start; i < signals.size(); ++i) {
            const auto& s = signals[i];
            ImGui::TableNextRow();

            ImGui::TableNextColumn(); ImGui::Text("%llu", (unsigned long long)s.timestamp_ms);
            ImGui::TableNextColumn();
            ImGui::TextColored(s.signal == Signal::BUY ? COL_GREEN : COL_RED,
                              s.signal == Signal::BUY ? "BUY" : "SELL");
            ImGui::TableNextColumn(); ImGui::Text("$%.2f", s.price);
            ImGui::TableNextColumn(); ImGui::TextColored(COL_DIM, "%s", s.reason.c_str());
        }
        ImGui::EndTable();
    }
}

void DashboardUI::render_backtest_panel() {
    ImGui::PanelHeader("BACKTEST");

    if (has_backtest_results_) {
        ImGui::TextColored(COL_GREEN, "Backtest completed successfully");

        if (ImGui::BeginTable("##bt", 2, ImGuiTableFlags_Borders)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored(COL_DIM, "Total Trades");
            ImGui::TableNextColumn(); ImGui::Text("%d", backtest_results_.total_trades);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored(COL_DIM, "Winning");
            ImGui::TableNextColumn(); ImGui::TextColored(COL_GREEN, "%d", backtest_results_.winning_trades);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored(COL_DIM, "Losing");
            ImGui::TableNextColumn(); ImGui::TextColored(COL_RED, "%d", backtest_results_.losing_trades);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored(COL_DIM, "Win Rate");
            ImGui::TableNextColumn(); ImGui::Text("%.1f%%", backtest_results_.win_rate * 100.0);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored(COL_DIM, "Total P&L");
            ImGui::TableNextColumn(); ImGui::TextColored(backtest_results_.total_pnl >= 0 ? COL_GREEN : COL_RED,
                          "$%.2f", backtest_results_.total_pnl);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored(COL_DIM, "Profit Factor");
            ImGui::TableNextColumn(); ImGui::Text("%.2f", backtest_results_.profit_factor);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored(COL_DIM, "Max Drawdown");
            ImGui::TableNextColumn(); ImGui::TextColored(COL_RED, "%.2f%%", backtest_results_.max_drawdown * 100.0);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored(COL_DIM, "Sharpe Ratio");
            ImGui::TableNextColumn(); ImGui::Text("%.2f", backtest_results_.sharpe_ratio);

            ImGui::EndTable();
        }
    } else {
        ImGui::TextColored(COL_DIM, "Click BACKTEST to run strategy backtest");
    }
}

void DashboardUI::reset_flags() {
    connect_requested_ = false;
    disconnect_requested_ = false;
    stop_requested_ = false;
    backtest_requested_ = false;
}