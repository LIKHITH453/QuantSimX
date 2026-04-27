#include "HFTPerf.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <vector>
#include <numeric>
#include <algorithm>
#include <cmath>

template<typename T>
double calc_mean(const std::vector<T>& v) {
    if (v.empty()) return 0.0;
    return std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
}

template<typename T>
double calc_percentile(const std::vector<T>& v, double p) {
    if (v.empty()) return 0.0;
    std::vector<T> sorted = v;
    std::sort(sorted.begin(), sorted.end());
    size_t idx = static_cast<size_t>(p * (sorted.size() - 1));
    return static_cast<double>(sorted[idx]);
}

template<typename T>
double calc_max(const std::vector<T>& v) {
    if (v.empty()) return 0.0;
    return *std::max_element(v.begin(), v.end());
}

struct alignas(64) Tick {
    double price;
    double qty;
    uint64_t ts_ns;
};

void test_spsc_ringbuffer() {
    std::cout << "Testing SPSCRingBuffer..." << std::endl;
    
    SPSCRingBuffer<Tick, 1024> rb;
    std::vector<uint64_t> latencies;
    
    for (int iter = 0; iter < 1000; ++iter) {
        Tick t{100.5, 1.0, static_cast<uint64_t>(iter)};
        
        uint64_t start = __builtin_readcyclecounter();
        bool ok = rb.push(t);
        uint64_t end = __builtin_readcyclecounter();
        
        if (ok) latencies.push_back(end - start);
        
        Tick out;
        if (rb.pop(out)) {
            assert(std::abs(out.price - 100.5) < 0.001);
        }
    }
    
    std::cout << "  Push latency (cycles): mean=" << calc_mean(latencies)
              << " p99=" << calc_percentile(latencies, 0.99)
              << " max=" << calc_max(latencies) << std::endl;
    
    assert(rb.empty());
    std::cout << "  PASSED" << std::endl;
}

void test_bitmask_orderbook() {
    std::cout << "Testing BitmaskOrderBook..." << std::endl;
    
    BitmaskOrderBook ob;
    ob.clear();
    
    ob.insert_bid(100.0, 10.0);
    ob.insert_bid(99.0, 5.0);
    ob.insert_bid(101.0, 8.0);
    ob.insert_ask(101.5, 3.0);
    ob.insert_ask(102.0, 7.0);
    
    assert(ob.best_bid() == 101.0);
    assert(ob.best_ask() == 101.5);
    assert(ob.spread() == 0.5);
    
    std::vector<uint64_t> bbo_latencies;
    for (int i = 0; i < 10000; ++i) {
        uint64_t start = __builtin_readcyclecounter();
        volatile double b = ob.best_bid();
        volatile double a = ob.best_ask();
        uint64_t end = __builtin_readcyclecounter();
        bbo_latencies.push_back(end - start);
        (void)b; (void)a;
    }
    
    std::cout << "  BBO latency (cycles): mean=" << calc_mean(bbo_latencies)
              << " p99=" << calc_percentile(bbo_latencies, 0.99)
              << " max=" << calc_max(bbo_latencies) << std::endl;
    
    std::cout << "  PASSED" << std::endl;
}

void test_cpu_pinner() {
    std::cout << "Testing CpuPinner..." << std::endl;
    
    int core = CpuPinner::core();
    std::cout << "  Running on core: " << core << std::endl;
    
    CpuPinner::pin(core);
    
    int core2 = CpuPinner::core();
    assert(core == core2);
    
    std::cout << "  PASSED" << std::endl;
}

void test_packed_order() {
    std::cout << "Testing PackedOrder..." << std::endl;
    
    PackedOrder o;
    o.id = 1;
    o.set_price(50000.00);
    o.set_qty(0.5);
    o.flags = 1;
    o.ts_ns = 1234567890;
    
    assert(o.is_bid() == true);
    assert(std::abs(o.price() - 50000.00) < 0.01);
    assert(std::abs(o.qty() - 0.5) < 0.00001);
    
    std::cout << "  sizeof(PackedOrder)=" << sizeof(PackedOrder) << " bytes" << std::endl;
    std::cout << "  PASSED" << std::endl;
}

void test_spsc_concurrent() {
    std::cout << "Testing SPSCRingBuffer concurrent (SPSC)..." << std::endl;
    
    SPSCRingBuffer<Tick, 4096> rb;
    std::atomic<int> processed{0};
    
    auto producer = [&]() {
        for (int i = 0; i < 100000; ++i) {
            Tick t{100.0 + i * 0.01, 1.0, static_cast<uint64_t>(i)};
            while (!rb.push(t)) { std::this_thread::yield(); }
        }
    };
    
    auto consumer = [&]() {
        Tick t;
        while (processed.load() < 100000) {
            if (rb.pop(t)) {
                processed.fetch_add(1);
            } else {
                std::this_thread::yield();
            }
        }
    };
    
    auto start = std::chrono::high_resolution_clock::now();
    std::thread p(producer);
    std::thread c(consumer);
    p.join();
    c.join();
    auto end = std::chrono::high_resolution_clock::now();
    
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "  100k items in " << dur << "ms" << std::endl;
    std::cout << "  Throughput: " << (100000.0 / dur) << " ops/ms" << std::endl;
    
    std::cout << "  PASSED" << std::endl;
}

int main() {
    std::cout << "=== QuantSimX Performance Tests ===" << std::endl << std::endl;
    
    test_packed_order();
    test_cpu_pinner();
    test_bitmask_orderbook();
    test_spsc_ringbuffer();
    test_spsc_concurrent();
    
    std::cout << std::endl << "=== All Tests Passed ===" << std::endl;
    return 0;
}