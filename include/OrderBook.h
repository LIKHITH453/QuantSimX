#pragma once

#include <vector>
#include <string>

class OrderBook {
public:
    OrderBook();
    void applyDepthUpdate(const std::string& json_msg);
    void applyTrade(const std::string& json_msg);
    void print_snapshot() const;
private:
    // Minimal in-memory representation: pairs of (price, size)
    std::vector<std::pair<double,double>> bids_;
    std::vector<std::pair<double,double>> asks_;
};
