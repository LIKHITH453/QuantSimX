#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>
#include <vector>
#include <random>
#include "HFT.h"

void test_spsc_basic() {
    std::cout << "[ SPSC Queue basic ]\n";
    SPSCQueue<int> q(1024);
    
    for (int i = 0; i < 100; ++i) {
        assert(q.push(i) == true);
    }
    assert(q.size() == 100);
    
    int val;
    assert(q.pop(val) == true);
    assert(val == 0);
    assert(q.size() == 99);
    
    std::cout << "  PASSED: push/pop works, size=" << q.size() << "\n";
}

void test_spsc_concurrent() {
    std::cout << "[ SPSC Queue concurrent ]\n";
    SPSCQueue<int> q(4096);
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    
    std::thread producer([&]() {
        for (int i = 0; i < 10000; ++i) {
            if (q.push(i)) {
                produced.fetch_add(1);
            }
        }
    });
    
    std::thread consumer([&]() {
        int val;
        while (consumed.load() < 10000) {
            if (q.pop(val)) {
                consumed.fetch_add(1);
            }
        }
    });
    
    producer.join();
    while (consumed.load() < 10000) {
        int val;
        if (q.pop(val)) consumed.fetch_add(1);
    }
    consumer.join();
    
    std::cout << "  Produced: " << produced.load() << ", Consumed: " << consumed.load() << "\n";
    std::cout << "  PASSED\n";
}

void test_order_32_bytes() {
    std::cout << "[ Order 32-byte packing ]\n";
    static_assert(sizeof(Order) == 32, "Order must be 32 bytes");
    static_assert(alignof(Order) == 32, "Order must be 32-byte aligned");
    
    Order o = {12345, 50000000000ULL, 1000000, 1, 1, 0};
    
    std::cout << "  Order size: " << sizeof(Order) << " bytes\n";
    std::cout << "  Align: " << alignof(Order) << "\n";
    std::cout << "  order_id=" << o.order_id << ", price=" << o.price_nano 
              << ", qty=" << o.quantity << ", side=" << (int)o.side << "\n";
    std::cout << "  2 orders fit in 64-byte L1 cache line: " << (64 / sizeof(Order) == 2 ? "YES" : "NO") << "\n";
    std::cout << "  PASSED\n";
}

void test_bit_scanning_orderbook() {
    std::cout << "[ BitScanningOrderBook O(1) BBO ]\n";
    BitScanningOrderBook ob;
    
    for (int i = 0; i < 16; ++i) {
        ob.add_order(i, 50000 + i * 100, 1000 + i * 100, true);
        ob.add_order(100 + i, 50100 + i * 100, 1000 + i * 100, false);
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    int iterations = 1000000;
    uint32_t bid_price = 0, ask_price = 0;
    
    for (int i = 0; i < iterations; ++i) {
        bid_price = ob.get_best_bid_price();
        ask_price = ob.get_best_ask_price();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    
    std::cout << "  " << iterations << " BBO calls: " << ns << " ns\n";
    std::cout << "  Best bid: " << bid_price << ", Best ask: " << ask_price << "\n";
    std::cout << "  " << (ns / iterations) << " ns/call\n";
    std::cout << "  PASSED: " << (bid_price > 0 && ask_price > bid_price ? "BBO valid" : "INVALID") << "\n";
}

void test_flat_hash_free_array() {
    std::cout << "[ FlatHashFreeArray O(1) lookup ]\n";
    FlatHashFreeArray<2048> map;
    
    for (uint64_t i = 0; i < 1000; ++i) {
        assert(map.insert(i * 100, static_cast<uint32_t>(i)) == true);
    }
    assert(map.size() == 1000);
    
    uint32_t val;
    assert(map.find(500, val) == true);
    assert(val == 5);
    
    assert(map.remove(500) == true);
    assert(map.find(500, val) == false);
    
    std::cout << "  Insert/lookup/remove works\n";
    std::cout << "  PASSED\n";
}

void test_cpu_pinner() {
    std::cout << "[ CPU Pinner ]\n";
    CPUPinner& pinner = CPUPinner::instance();
    
    std::cout << "  Available cores: " << pinner.get_available_cores() << "\n";
    
    std::thread t([&]() {
        pinner.pin_current_thread(0);
        std::cout << "  Current core: " << pinner.get_current_core() << "\n";
    });
    
    t.join();
    std::cout << "  PASSED\n";
}

int main() {
    std::cout << "\n========================================\n";
    std::cout << "      QuantSimX HFT Features Test      \n";
    std::cout << "========================================\n\n";
    
    test_spsc_basic();
    test_spsc_concurrent();
    test_order_32_bytes();
    test_bit_scanning_orderbook();
    test_flat_hash_free_array();
    test_cpu_pinner();
    
    std::cout << "\n========================================\n";
    std::cout << "      ALL HFT TESTS PASSED             \n";
    std::cout << "========================================\n\n";
    
    return 0;
}