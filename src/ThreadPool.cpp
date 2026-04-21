#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t num_threads) {
    workers_.reserve(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this] { worker_thread(); });
    }
}

ThreadPool::~ThreadPool() {
    stop_.store(true);
    condition_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::worker_thread() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            condition_.wait(lock, [this] { return stop_.load() || !tasks_.empty(); });
            
            if (stop_.load() && tasks_.empty()) {
                return;
            }
            
            if (!tasks_.empty()) {
                task = std::move(tasks_.front());
                tasks_.pop();
                active_tasks_.fetch_add(1);
            }
        }
        
        if (task) {
            task();
            active_tasks_.fetch_sub(1);
            completion_condition_.notify_one();
        }
    }
}

void ThreadPool::wait_all() {
    std::unique_lock<std::mutex> lock(completion_mutex_);
    completion_condition_.wait(lock, [this] { return tasks_.empty() && active_tasks_.load() == 0; });
}
