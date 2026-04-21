#include "OrderBook.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

OrderBook::OrderBook() = default;

void OrderBook::init_from_snapshot(const json& snapshot_json) {
    bid_count_ = 0;
    ask_count_ = 0;
    
    if (snapshot_json.contains("bids")) {
        for (const auto& level : snapshot_json["bids"]) {
            double price = std::stod(level[0].get<std::string>());
            double qty = std::stod(level[1].get<std::string>());
            if (qty > 0 && bid_count_ < MAX_LEVELS) {
                insert_bid(price, qty);
            }
        }
    }
    
    if (snapshot_json.contains("asks")) {
        for (const auto& level : snapshot_json["asks"]) {
            double price = std::stod(level[0].get<std::string>());
            double qty = std::stod(level[1].get<std::string>());
            if (qty > 0 && ask_count_ < MAX_LEVELS) {
                insert_ask(price, qty);
            }
        }
    }
}

void OrderBook::apply_depth_update(const json& depth_msg) {
    if (depth_msg.contains("b")) {
        for (const auto& level : depth_msg["b"]) {
            double price = std::stod(level[0].get<std::string>());
            double qty = std::stod(level[1].get<std::string>());
            
            if (qty == 0) {
                for (std::size_t i = 0; i < bid_count_; ++i) {
                    if (bids_[i].price == price) {
                        erase_bid(i);
                        break;
                    }
                }
            } else {
                bool found = false;
                for (std::size_t i = 0; i < bid_count_; ++i) {
                    if (bids_[i].price == price) {
                        bids_[i].quantity = qty;
                        found = true;
                        break;
                    }
                }
                if (!found && bid_count_ < MAX_LEVELS) {
                    insert_bid(price, qty);
                }
            }
        }
    }
    
    if (depth_msg.contains("a")) {
        for (const auto& level : depth_msg["a"]) {
            double price = std::stod(level[0].get<std::string>());
            double qty = std::stod(level[1].get<std::string>());
            
            if (qty == 0) {
                for (std::size_t i = 0; i < ask_count_; ++i) {
                    if (asks_[i].price == price) {
                        erase_ask(i);
                        break;
                    }
                }
            } else {
                bool found = false;
                for (std::size_t i = 0; i < ask_count_; ++i) {
                    if (asks_[i].price == price) {
                        asks_[i].quantity = qty;
                        found = true;
                        break;
                    }
                }
                if (!found && ask_count_ < MAX_LEVELS) {
                    insert_ask(price, qty);
                }
            }
        }
    }
    
    if (depth_msg.contains("U")) {
        last_update_id_ = depth_msg["U"].get<uint64_t>();
    }
}

void OrderBook::apply_aggtrade(const json& trade_msg) {
    if (trade_msg.contains("p") && trade_msg.contains("q")) {
        last_trade_price_ = std::stod(trade_msg["p"].get<std::string>());
        last_trade_qty_ = std::stod(trade_msg["q"].get<std::string>());
        if (trade_msg.contains("T")) {
            last_trade_time_ = trade_msg["T"].get<uint64_t>();
        }
    }
}

void OrderBook::insert_bid(double price, double qty) {
    std::size_t i = 0;
    while (i < bid_count_ && bids_[i].price > price) {
        ++i;
    }
    if (i < MAX_LEVELS) {
        for (std::size_t j = bid_count_; j > i; --j) {
            bids_[j] = bids_[j - 1];
        }
        bids_[i] = {price, qty};
        if (bid_count_ < MAX_LEVELS) ++bid_count_;
    }
}

void OrderBook::insert_ask(double price, double qty) {
    std::size_t i = 0;
    while (i < ask_count_ && asks_[i].price < price) {
        ++i;
    }
    if (i < MAX_LEVELS) {
        for (std::size_t j = ask_count_; j > i; --j) {
            asks_[j] = asks_[j - 1];
        }
        asks_[i] = {price, qty};
        if (ask_count_ < MAX_LEVELS) ++ask_count_;
    }
}

void OrderBook::erase_bid(std::size_t idx) {
    for (std::size_t i = idx; i < bid_count_ - 1; ++i) {
        bids_[i] = bids_[i + 1];
    }
    --bid_count_;
}

void OrderBook::erase_ask(std::size_t idx) {
    for (std::size_t i = idx; i < ask_count_ - 1; ++i) {
        asks_[i] = asks_[i + 1];
    }
    --ask_count_;
}

void OrderBook::print_snapshot() const {
    std::cout << "\n=== OrderBook Snapshot ===\n";
    std::cout << "Best bid: " << std::fixed << std::setprecision(2) << get_best_bid() << "\n";
    std::cout << "Best ask: " << get_best_ask() << "\n";
    std::cout << "Mid price: " << get_mid_price() << "\n";
    
    std::cout << "\nTop 5 Bids:\n";
    for (std::size_t i = 0; i < std::min(std::size_t(5), bid_count_); ++i) {
        std::cout << "  " << bids_[i].price << " x " << bids_[i].quantity << "\n";
    }
    
    std::cout << "\nTop 5 Asks:\n";
    for (std::size_t i = 0; i < std::min(std::size_t(5), ask_count_); ++i) {
        std::cout << "  " << asks_[i].price << " x " << asks_[i].quantity << "\n";
    }
    std::cout << "========================\n\n";
}