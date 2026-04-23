#include "ThreadPool.h"
#include <chrono>

ThreadPool::ThreadPool(size_t num_threads) {
    workers_.reserve(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this, i] { worker_thread(i); });
    }
}

ThreadPool::~ThreadPool() {
    stop_.store(true);
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::worker_thread(std::size_t worker_id) {
    (void)worker_id;
    while (!stop_.load()) {
        std::size_t tail = tail_.load(std::memory_order_acquire);
        std::size_t head = head_.load(std::memory_order_acquire);
        
        if (tail <= head) {
            if (stop_.load()) return;
            std::this_thread::yield();
            continue;
        }
        
        std::size_t idx = tail % MAX_TASKS;
        Task& t = tasks_[idx];
        
        if (t.taken.load(std::memory_order_acquire)) {
            std::this_thread::yield();
            continue;
        }
        
        t.taken.store(true, std::memory_order_release);
        tail_.store(tail + 1, std::memory_order_release);
        
        if (t.func) {
            t.func();
        }
        
        task_count_.fetch_sub(1, std::memory_order_release);
    }
}

void ThreadPool::wait_all() {
    while (task_count_.load() > 0) {
        std::this_thread::yield();
    }
}