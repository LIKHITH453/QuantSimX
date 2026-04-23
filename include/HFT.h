#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

template<typename T>
class SPSCQueue {
public:
    explicit SPSCQueue(std::size_t capacity) 
        : capacity_(next_pow2(capacity))
        , buffer_(new T[capacity_])
    {
        static_assert(std::is_trivially_destructible_v<T>, "T must be trivially destructible");
    }
    
    ~SPSCQueue() { delete[] buffer_; }
    
    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;

    bool push(const T& item) {
        std::size_t write_idx = write_pos_.load(std::memory_order_relaxed);
        std::size_t read_idx = read_pos_.load(std::memory_order_acquire);
        
        if (write_idx - read_idx >= capacity_) {
            return false;
        }
        
        buffer_[write_idx & (capacity_ - 1)] = item;
        
        std::atomic_thread_fence(std::memory_order_release);
        
        write_pos_.store(write_idx + 1, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        std::size_t read_idx = read_pos_.load(std::memory_order_relaxed);
        std::size_t write_idx = write_pos_.load(std::memory_order_acquire);
        
        if (write_idx <= read_idx) {
            return false;
        }
        
        std::atomic_thread_fence(std::memory_order_acquire);
        
        item = buffer_[read_idx & (capacity_ - 1)];
        
        read_pos_.store(read_idx + 1, std::memory_order_release);
        return true;
    }

    bool is_empty() const {
        return write_pos_.load(std::memory_order_acquire) <= read_pos_.load(std::memory_order_acquire);
    }

    std::size_t size() const {
        std::size_t write = write_pos_.load(std::memory_order_acquire);
        std::size_t read = read_pos_.load(std::memory_order_acquire);
        return write > read ? write - read : 0;
    }

private:
    static std::size_t next_pow2(std::size_t n) {
        --n;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        n |= n >> 32;
        return n + 1;
    }
    
    const std::size_t capacity_;
    T* const buffer_;
    
    alignas(64) std::atomic<std::size_t> write_pos_{0};
    alignas(64) std::atomic<std::size_t> read_pos_{0};
};

struct alignas(32) Order {
    uint64_t order_id;
    uint32_t price_nano;
    uint32_t quantity;
    uint32_t side : 1;
    uint32_t status : 2;
    uint32_t padding : 29;
};

static_assert(sizeof(Order) == 32, "Order must be exactly 32 bytes (fits 2 per L1 cache line)");
static_assert(alignof(Order) == 32, "Order must be 32-byte aligned");

struct OrderBookLevel {
    uint32_t price_nano;
    uint32_t quantity;
};

constexpr std::size_t MAX_PRICE_LEVELS = 64;
constexpr std::size_t MAX_ORDERS = 1024;

class BitScanningOrderBook {
public:
    BitScanningOrderBook() = default;
    
    void reset() {
        bid_levels_.fill({0, 0});
        ask_levels_.fill({0, 0});
        bid_count_ = 0;
        ask_count_ = 0;
        bid_bitmask_ = 0;
        ask_bitmask_ = 0;
        orders_.fill({});
        order_count_ = 0;
    }
    
    int add_order(uint64_t order_id, uint32_t price_nano, uint32_t quantity, bool is_bid) {
        if (order_count_ >= MAX_ORDERS) return -1;
        
        std::size_t level_idx = find_empty_level(is_bid);
        if (level_idx == MAX_PRICE_LEVELS) return -1;
        
        if (is_bid) {
            bid_levels_[level_idx] = {price_nano, quantity};
            bid_bitmask_ |= (1ULL << level_idx);
            ++bid_count_;
        } else {
            ask_levels_[level_idx] = {price_nano, quantity};
            ask_bitmask_ |= (1ULL << level_idx);
            ++ask_count_;
        }
        
        orders_[order_count_] = {order_id, price_nano, quantity, is_bid ? 1 : 0, 1, 0};
        std::size_t order_idx = order_count_;
        ++order_count_;
        
        return static_cast<int>(order_idx);
    }
    
    void remove_order(uint64_t order_id) {
        for (std::size_t i = 0; i < order_count_; ++i) {
            if (orders_[i].order_id == order_id && orders_[i].status == 1) {
                bool is_bid = orders_[i].side == 1;
                uint32_t price = orders_[i].price_nano;
                
                orders_[i].status = 2;
                
                for (std::size_t l = 0; l < MAX_PRICE_LEVELS; ++l) {
                    if ((is_bid ? bid_levels_[l].price_nano : ask_levels_[l].price_nano) == price) {
                        if (is_bid) {
                            bid_bitmask_ &= ~(1ULL << l);
                            --bid_count_;
                        } else {
                            ask_bitmask_ &= ~(1ULL << l);
                            --ask_count_;
                        }
                        break;
                    }
                }
                break;
            }
        }
    }
    
    int get_best_bid_level() const {
        if (bid_bitmask_ == 0) return -1;
        return __builtin_ctzll(bid_bitmask_);
    }
    
    int get_best_ask_level() const {
        if (ask_bitmask_ == 0) return -1;
        return __builtin_ctzll(ask_bitmask_);
    }
    
    uint32_t get_best_bid_price() const {
        int lvl = get_best_bid_level();
        return lvl >= 0 ? bid_levels_[lvl].price_nano : 0;
    }
    
    uint32_t get_best_ask_price() const {
        int lvl = get_best_ask_level();
        return lvl >= 0 ? ask_levels_[lvl].price_nano : 0;
    }
    
    int get_best_bid_qty() const {
        int lvl = get_best_bid_level();
        return lvl >= 0 ? bid_levels_[lvl].quantity : 0;
    }
    
    int get_best_ask_qty() const {
        int lvl = get_best_ask_level();
        return lvl >= 0 ? ask_levels_[lvl].quantity : 0;
    }

    Order* find_order(uint64_t order_id) {
        for (std::size_t i = 0; i < order_count_; ++i) {
            if (orders_[i].order_id == order_id) {
                return &orders_[i];
            }
        }
        return nullptr;
    }

private:
    std::size_t find_empty_level(bool is_bid) {
        uint64_t bitmask = is_bid ? bid_bitmask_ : ask_bitmask_;
        uint64_t inv = ~bitmask;
        if (inv == 0) return MAX_PRICE_LEVELS;
        return __builtin_ctzll(inv);
    }
    
    alignas(64) std::array<OrderBookLevel, MAX_PRICE_LEVELS> bid_levels_;
    alignas(64) std::array<OrderBookLevel, MAX_PRICE_LEVELS> ask_levels_;
    alignas(64) uint64_t bid_bitmask_ = 0;
    alignas(64) uint64_t ask_bitmask_ = 0;
    std::size_t bid_count_ = 0;
    std::size_t ask_count_ = 0;
    
    alignas(64) std::array<Order, MAX_ORDERS> orders_;
    std::size_t order_count_ = 0;
};

class CPUPinner {
public:
    static constexpr std::size_t MAX_CORES = 64;
    
    CPUPinner() {
        std::memset(available_cores_, 0, sizeof(available_cores_));
        std::memset(core_to_thread_, 0, sizeof(core_to_thread_));
        discovered_ = discover_cores();
    }
    
    static CPUPinner& instance() {
        static CPUPinner pinner;
        return pinner;
    }
    
    bool pin_thread_to_core(std::thread& th, std::size_t core_id) {
#if defined(__linux__)
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core_id, &cpuset);
        
        int result = pthread_setaffinity_np(th.native_handle(), sizeof(cpu_set_t), &cpuset);
        if (result == 0) {
            core_to_thread_[core_id] = th.get_id();
            return true;
        }
#endif
        return false;
    }
    
    void pin_current_thread(std::size_t core_id) {
#if defined(__linux__)
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core_id, &cpuset);
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
        core_to_thread_[core_id] = std::this_thread::get_id();
#endif
    }
    
    std::size_t get_available_cores() const { return discovered_; }
    
    std::size_t get_current_core() const {
#if defined(__linux__)
        return sched_getcpu();
#else
        return 0;
#endif
    }

private:
    std::size_t discover_cores() {
#if defined(__linux__)
        std::size_t cores = 0;
        FILE* f = fopen("/proc/cpuinfo", "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                if (std::strncmp(line, "processor", 9) == 0) {
                    ++cores;
                }
            }
            fclose(f);
        }
        return cores > 0 ? cores : std::thread::hardware_concurrency();
#else
        return std::thread::hardware_concurrency();
#endif
    }
    
    bool available_cores_[MAX_CORES];
    std::thread::id core_to_thread_[MAX_CORES];
    std::size_t discovered_ = 0;
};

template<std::size_t Capacity>
class FlatHashFreeArray {
public:
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    
    struct Entry {
        uint64_t key;
        uint32_t value;
        bool occupied;
    };
    
    FlatHashFreeArray() {
        for (auto& e : entries_) {
            e.key = 0;
            e.value = 0;
            e.occupied = false;
        }
    }
    
    bool insert(uint64_t key, uint32_t value) {
        std::size_t idx = key & (Capacity - 1);
        
        if (entries_[idx].occupied && entries_[idx].key != key) {
            return false;
        }
        
        entries_[idx].key = key;
        entries_[idx].value = value;
        entries_[idx].occupied = true;
        ++count_;
        return true;
    }
    
    bool find(uint64_t key, uint32_t& value) const {
        std::size_t idx = key & (Capacity - 1);
        
        if (!entries_[idx].occupied || entries_[idx].key != key) {
            return false;
        }
        
        value = entries_[idx].value;
        return true;
    }
    
    bool remove(uint64_t key) {
        std::size_t idx = key & (Capacity - 1);
        
        if (!entries_[idx].occupied || entries_[idx].key != key) {
            return false;
        }
        
        entries_[idx].occupied = false;
        --count_;
        return true;
    }
    
    void clear() {
        for (auto& e : entries_) {
            e.occupied = false;
        }
        count_ = 0;
    }
    
    std::size_t size() const { return count_; }
    bool empty() const { return count_ == 0; }

private:
    alignas(64) std::array<Entry, Capacity> entries_;
    std::size_t count_ = 0;
};

#if defined(__linux__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

class ZeroCopyUDPSocket {
public:
    ZeroCopyUDPSocket() : fd_(-1), buffer_(nullptr) {}
    
    ~ZeroCopyUDPSocket() { close(); }
    
    bool bind(uint16_t port) {
        fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd_ < 0) return false;
        
        int rcvbuf = 16777216;
        setsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
        
        int zero = 1;
        setsockopt(fd_, IPPROTO_IP, IP_RECVDSTADDR, &zero, sizeof(zero));
        
        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;
        
        if (::bind(fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close();
            return false;
        }
        
        buffer_ = new char[65536];
        return true;
    }
    
    struct alignas(64) [[gnu::packed]]UDPPacket {
        uint32_t source_ip;
        uint16_t source_port;
        uint16_t dest_port;
        uint32_t length;
        uint32_t checksum;
        char payload[65536 - 24];
    };
    
    ssize_t recv_raw(void* dest, std::size_t max_len) {
        char cmsg_buf[256];
        
        struct iovec iov;
        iov.iov_base = buffer_;
        iov.iov_len = 65536;
        
        struct msghdr msg;
        msg.msg_name = nullptr;
        msg.msg_namelen = 0;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = cmsg_buf;
        msg.msg_controllen = sizeof(cmsg_buf);
        msg.msg_flags = 0;
        
        ssize_t ret = recvmsg(fd_, &msg, 0);
        if (ret <= 0) return ret;
        
        if (static_cast<std::size_t>(ret) > max_len) {
            ret = static_cast<std::size_t>(max_len);
        }
        
        std::memcpy(dest, buffer_, ret);
        return ret;
    }
    
    void* get_buffer() { return buffer_; }
    
    bool is_valid() const { return fd_ >= 0; }
    
    void close() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
        delete[] buffer_;
        buffer_ = nullptr;
    }

private:
    int fd_;
    char* buffer_;
};

#else

class ZeroCopyUDPSocket {
public:
    ZeroCopyUDPSocket() : fd_(-1) {}
    bool bind(uint16_t) { return false; }
    ssize_t recv_raw(void*, std::size_t) { return -1; }
    void* get_buffer() { return nullptr; }
    bool is_valid() const { return false; }
    void close() { fd_ = -1; }
private:
    int fd_;
};

#endif

using TickSPSC = SPSCQueue<QueuedTick>;
using OrderSPSC = SPSCQueue<Order>;
using OrderIDMap = FlatHashFreeArray<2048>;