#pragma once

#include <thread>
#include <atomic>
#include <string>
#include "OrderBook.h"

class NetworkClient {
public:
    explicit NetworkClient(OrderBook& ob);
    ~NetworkClient();
    void start();
    void stop();

private:
    void run_simulator();

    OrderBook& ob_;
    std::thread thread_;
    std::atomic<bool> running_ = false;
};
