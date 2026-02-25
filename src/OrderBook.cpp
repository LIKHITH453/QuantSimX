#include "OrderBook.h"
#include <iostream>

OrderBook::OrderBook() = default;

void OrderBook::applyDepthUpdate(const std::string& /*json_msg*/) {
    // TODO: parse depth diff JSON and update bids_/asks_
}

void OrderBook::applyTrade(const std::string& /*json_msg*/) {
    // TODO: parse trade JSON and update any trade-related state
}

void OrderBook::print_snapshot() const {
    std::cout << "OrderBook snapshot (stub)\n";
    std::cout << "Bids: " << bids_.size() << " entries\n";
    std::cout << "Asks: " << asks_.size() << " entries\n";
}
