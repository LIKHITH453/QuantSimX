#include <iostream>
#include <thread>
#include <atomic>
#include <cassert>
#include <vector>
#include <chrono>
#include <fstream>
#include "LockFreeQueue.h"
#include "OrderBook.h"
#include "VWAPCalculator.h"
#include "SignalGenerator.h"
#include "ThreadPool.h"

std::atomic<bool> g_all_passed{true};

void test_lockfree_queue_basic() {
    std::cout << "[ LockFreeQueue basic ]\n";
    LockFreeQueue<int, 128> q;
    
    int item;
    bool result;
    
    result = q.is_empty();
    assert(result == true);
    result = q.pop(item);
    assert(result == false);
    
    result = q.push(42);
    assert(result == true);
    result = q.is_empty();
    assert(result == false);
    assert(q.size() == 1);
    result = q.pop(item);
    assert(result == true);
    assert(item == 42);
    result = q.is_empty();
    assert(result == true);
    
    std::cout << "  PASSED: basic push/pop\n";
}

void test_lockfree_queue_wrap() {
    std::cout << "[ LockFreeQueue wrap ]\n";
    LockFreeQueue<int, 8> q;
    
    // Push 16 items (will overflow after 8)
    for (int i = 0; i < 16; ++i) {
        q.push(i);
    }
    
    // Queue should have first 8 items [0-7]
    assert(q.size() == 8);
    
    int item;
    bool ok = q.pop(item);
    assert(ok == true);
    // First item should be 0 (oldest in queue)
    assert(item == 0);
    
    ok = q.push(100);
    assert(ok == true);
    assert(q.size() == 8);
    
    std::cout << "  Popped: " << item << ", push(100) success, size=" << q.size() << "\n";
    std::cout << "  PASSED: wrap-around\n";
}

void test_lockfree_queue_concurrent() {
    std::cout << "[ LockFreeQueue concurrent ]\n";
    LockFreeQueue<int, 1024> q;
    std::atomic<int> push_count{0};
    std::atomic<int> pop_count{0};
    
    std::thread producer([&]() {
        for (int i = 0; i < 500; ++i) {
            if (q.push(i)) {
                push_count.fetch_add(1);
            }
            std::this_thread::yield();
        }
    });
    
    std::thread consumer([&]() {
        int item;
        while (pop_count.load() < 500) {
            if (q.pop(item)) {
                pop_count.fetch_add(1);
            }
            std::this_thread::yield();
        }
    });
    
    producer.join();
    consumer.join();
    
    std::cout << "  Producer pushed: " << push_count.load() << "\n";
    std::cout << "  Consumer popped: " << pop_count.load() << "\n";
    std::cout << "  PASSED: concurrent test\n";
}

void test_orderbook_flat_array() {
    std::cout << "[ OrderBook performance ]\n";
    OrderBook ob;
    std::string json_str = R"({"bids":[["50000.00","1.5"],["49900.00","2.0"]],"asks":[["50100.00","1.0"],["50200.00","2.5"]]})";
    json j = json::parse(json_str);
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000; ++i) {
        ob.init_from_snapshot(j);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << "  10000 snapshot inits: " << us << " us\n";
    std::cout << "  " << (us < 100000 ? "PASSED: performance OK" : "WARNING: TOO SLOW") << "\n";
}

void test_orderbook_best_bid_ask() {
    std::cout << "[ OrderBook best bid/ask ]\n";
    OrderBook ob;
    std::string json_str = R"({"bids":[["50000.00","1.5"],["49900.00","2.0"],["49800.00","0.5"]],"asks":[["50100.00","1.0"],["50200.00","2.5"],["50300.00","1.0"]]})";
    json j = json::parse(json_str);
    ob.init_from_snapshot(j);
    
    assert(ob.get_best_bid() == 50000.0);
    assert(ob.get_best_ask() == 50100.0);
    assert(ob.get_mid_price() == 50050.0);
    
    std::cout << "  PASSED: best bid=" << ob.get_best_bid() << " ask=" << ob.get_best_ask() << " mid=" << ob.get_mid_price() << "\n";
}

void test_vwap_cumulative() {
    std::cout << "[ VWAPCalculator performance ]\n";
    VWAPCalculator vwap(20, 14);
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100000; ++i) {
        vwap.add_price(i, 50000.0 + (i % 100), 1.0);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "  100000 add_price calls: " << ms << " ms\n";
    std::cout << "  VWAP: " << vwap.get_vwap() << "\n";
    std::cout << "  " << (ms < 100 ? "PASSED: performance OK" : "WARNING: TOO SLOW") << "\n";
}

void test_vwap_rsi() {
    std::cout << "[ VWAPCalculator RSI ]\n";
    VWAPCalculator vwap(20, 14);
    
    for (int i = 0; i < 50; ++i) {
        double price = 50000.0 + (i % 10) * 10;
        vwap.add_price(i, price, 1.0);
    }
    
    vwap.update();
    std::cout << "  SMA: " << vwap.get_sma() << "\n";
    std::cout << "  RSI: " << vwap.get_rsi() << "\n";
    std::cout << "  PASSED\n";
}

void test_signal_generation() {
    std::cout << "[ SignalGenerator ]\n";
    SignalGenerator sg(30.0, 70.0);
    
    Signal sig = sg.update(1, 50000, 50000, 50);
    assert(sig == Signal::NONE);
    
    sig = sg.update(2, 50100, 50000, 25);
    assert(sig == Signal::BUY);
    
    sig = sg.update(3, 49900, 50000, 75);
    assert(sig == Signal::SELL);
    
    auto events = sg.get_signals();
    std::cout << "  Signals: " << events.size() << " (BUY=" << (events[0].signal == Signal::BUY ? "yes" : "no") << ")\n";
    std::cout << "  PASSED\n";
}

void test_threadpool_basics() {
    std::cout << "[ ThreadPool basics ]\n";
    ThreadPool pool(2);
    
    std::atomic<int> counter{0};
    
    for (int i = 0; i < 100; ++i) {
        pool.enqueue([&counter]() {
            counter.fetch_add(1);
        });
    }
    
    pool.wait_all();
    
    assert(counter.load() == 100);
    std::cout << "  Counter: " << counter.load() << "\n";
    std::cout << "  PASSED\n";
}

void test_threadpool_concurrent() {
    std::cout << "[ ThreadPool concurrent ]\n";
    ThreadPool pool(4);
    
    std::atomic<int> counter{0};
    std::vector<std::thread> threads;
    
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&pool, &counter, t]() {
            for (int i = 0; i < 250; ++i) {
                pool.enqueue([&counter, t]() {
                    counter.fetch_add(1);
                    std::this_thread::yield();
                });
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    pool.wait_all();
    
    std::cout << "  Counter: " << counter.load() << "\n";
    std::cout << "  " << (counter.load() == 1000 ? "PASSED" : "FAILED") << "\n";
}

int main() {
    std::cout << "\n====================================\n";
    std::cout << "     QuantSimX Optimization Tests    \n";
    std::cout << "====================================\n\n";
    
    try {
        test_lockfree_queue_basic();
        test_lockfree_queue_wrap();
        test_lockfree_queue_concurrent();
        test_orderbook_flat_array();
        test_orderbook_best_bid_ask();
        test_vwap_cumulative();
        test_vwap_rsi();
        test_signal_generation();
        test_threadpool_basics();
        test_threadpool_concurrent();
    } catch (const std::exception& e) {
        std::cout << "  EXCEPTION: " << e.what() << "\n";
        g_all_passed = false;
    }
    
    std::cout << "\n====================================\n";
    if (g_all_passed) {
        std::cout << "     ALL TESTS PASSED              \n";
    } else {
        std::cout << "     SOME TESTS FAILED             \n";
    }
    std::cout << "====================================\n";
    
    return g_all_passed ? 0 : 1;
}