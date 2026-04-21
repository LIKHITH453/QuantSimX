#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPool();
    
    template<typename F>
    void enqueue(F&& task);
    
    void wait_all();
    size_t size() const { return workers_.size(); }
    
private:
    void worker_thread();
    
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_{false};
    std::atomic<size_t> active_tasks_{0};
    std::mutex completion_mutex_;
    std::condition_variable completion_condition_;
};

template<typename F>
void ThreadPool::enqueue(F&& task) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        tasks_.emplace(std::forward<F>(task));
    }
    condition_.notify_one();
}

#endif
