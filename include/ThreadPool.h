#pragma once

#include <atomic>
#include <array>
#include <thread>
#include <vector>
#include <functional>
#include <cstddef>

class ThreadPool {
public:
    static constexpr std::size_t MAX_TASKS = 4096;
    
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPool();
    
    template<typename F>
    bool enqueue(F&& task);
    
    void wait_all();
    size_t size() const { return workers_.size(); }
    bool is_empty() const { return task_count_.load() == 0; }

private:
    struct Task {
        std::atomic<bool> taken{false};
        std::function<void()> func;
    };
    
    void worker_thread(std::size_t worker_id);
    
    std::vector<std::thread> workers_;
    std::array<Task, MAX_TASKS> tasks_;
    std::atomic<std::size_t> head_{0};
    std::atomic<std::size_t> tail_{0};
    std::atomic<std::size_t> task_count_{0};
    std::atomic<bool> stop_{false};
    std::atomic<std::size_t> num_waiting_{0};
    std::atomic<std::size_t> generation_{0};
};

template<typename F>
bool ThreadPool::enqueue(F&& task) {
    if (stop_.load()) return false;
    
    std::size_t head = head_.load(std::memory_order_relaxed);
    std::size_t tail = tail_.load(std::memory_order_acquire);
    std::size_t count = task_count_.load(std::memory_order_relaxed);
    
    if (count >= MAX_TASKS) return false;
    
    std::size_t idx = head % MAX_TASKS;
    Task& t = tasks_[idx];
    
    while (t.taken.load(std::memory_order_acquire)) {
        if (count >= MAX_TASKS) return false;
        tail = tail_.load(std::memory_order_acquire);
        head = head_.load(std::memory_order_relaxed);
        idx = head % MAX_TASKS;
    }
    
    t.func = std::forward<F>(task);
    t.taken.store(false, std::memory_order_release);
    
    head_.store(head + 1, std::memory_order_release);
    task_count_.fetch_add(1, std::memory_order_release);
    
    return true;
}