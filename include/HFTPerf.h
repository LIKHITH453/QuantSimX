#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <cstring>
#include <unistd.h>
#include <sched.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

template<typename T, std::size_t Capacity>
class alignas(64) SPSCRingBuffer {
public:
    static_assert((Capacity & (Capacity - 1)) == 0, "Power of 2 required");
    static_assert(std::is_trivially_destructible_v<T>, "Trivially destructible required");

    bool push(const T& item) {
        std::size_t w = write_idx_.load(std::memory_order_relaxed);
        std::size_t r = read_idx_.load(std::memory_order_acquire);
        if (w - r >= Capacity) return false;
        slots_[w & (Capacity - 1)] = item;
        std::atomic_thread_fence(std::memory_order_release);
        write_idx_.store(w + 1, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        std::size_t r = read_idx_.load(std::memory_order_relaxed);
        std::size_t w = write_idx_.load(std::memory_order_acquire);
        if (w == r) return false;
        std::atomic_thread_fence(std::memory_order_acquire);
        item = slots_[r & (Capacity - 1)];
        read_idx_.store(r + 1, std::memory_order_release);
        return true;
    }

    bool empty() const { return write_idx_.load(std::memory_order_acquire) == read_idx_.load(std::memory_order_acquire); }
    std::size_t size() const { std::size_t w = write_idx_.load(std::memory_order_acquire), r = read_idx_.load(std::memory_order_acquire); return w > r ? w - r : 0; }

private:
    alignas(64) std::array<T, Capacity> slots_;
    alignas(64) std::atomic<std::size_t> write_idx_{0};
    alignas(64) std::atomic<std::size_t> read_idx_{0};
};

class CpuPinner {
public:
    static bool pin(int core) { cpu_set_t c; CPU_ZERO(&c); CPU_SET(core, &c); return pthread_setaffinity_np(pthread_self(), sizeof(c), &c) == 0; }
    static int core() { return sched_getcpu(); }
};

#pragma pack(push, 1)
struct PackedOrder {
    uint32_t id;
    uint32_t price_raw;
    uint32_t qty_raw;
    uint32_t flags;
    uint64_t ts_ns;
    bool is_bid() const { return flags & 1; }
    double price() const { return price_raw * 0.01; }
    double qty() const { return qty_raw * 0.00001; }
    void set_price(double p) { price_raw = static_cast<uint32_t>(p * 100 + 0.5); }
    void set_qty(double q) { qty_raw = static_cast<uint32_t>(q * 100000 + 0.5); }
};
#pragma pack(pop)
static_assert(sizeof(PackedOrder) == 24);

#pragma pack(push, 1)
struct NetworkPacket {
    uint64_t ts_ns;
    uint32_t id;
    uint32_t price_raw;
    uint32_t qty_raw;
    uint32_t flags;
    uint32_t pad1;
    uint32_t pad2;
    uint32_t pad3;
};
#pragma pack(pop)

struct alignas(64) MarketData {
    uint64_t ts_ns, seq;
    double bid, ask, bid_qty, ask_qty, last_px, last_qty;
    bool valid() const { return ts_ns > 0; }
};

class alignas(64) ZeroCopyUDP {
public:
    ZeroCopyUDP() : fd_(-1), rx_(0), drop_(0), last_seq_(0) {}
    ~ZeroCopyUDP() { close(); }

    bool bind(int port) {
        fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (fd_ < 0) return false;
        int yes = 1; ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(port); a.sin_addr.s_addr = INADDR_ANY;
        if (::bind(fd_, (sockaddr*)&a, sizeof(a)) < 0) { ::close(fd_); fd_ = -1; return false; }
        ::fcntl(fd_, F_SETFL, ::fcntl(fd_, F_GETFL) | O_NONBLOCK);
        return true;
    }

    int recv(MarketData* out) {
        char buf[1024];
        sockaddr_in from; socklen_t len = sizeof(from);
        ssize_t n = ::recvfrom(fd_, buf, sizeof(buf), 0, (sockaddr*)&from, &len);
        if (n <= 0) return (errno == EAGAIN || errno == EWOULDBLOCK) ? 0 : -1;
        if (n < 48) { ++drop_; return -1; }
        auto w = (uint64_t*)buf;
        out->ts_ns = w[0];
        out->bid = ((double*)&w[1])[0]; out->ask = ((double*)&w[1])[1];
        out->bid_qty = ((double*)&w[1])[2]; out->ask_qty = ((double*)&w[1])[3];
        out->last_px = ((double*)&w[1])[4]; out->last_qty = ((double*)&w[1])[5];
        out->seq = w[7];
        if (out->seq <= last_seq_) { ++drop_; return -1; }
        last_seq_ = out->seq;
        ++rx_; return 1;
    }

    void close() { if (fd_ >= 0) { ::close(fd_); fd_ = -1; } }
    int fd() const { return fd_; }
    uint64_t rx() const { return rx_; }
    uint64_t drops() const { return drop_; }

private:
    int fd_;
    uint64_t rx_, drop_, last_seq_;
};

constexpr int MAX_LEVELS = 64;

class alignas(64) BitmaskOrderBook {
public:
    void clear() { bid_mask_ = ask_mask_ = bid_cnt_ = ask_cnt_ = 0; }

    void insert_bid(double px, double qty) {
        if (bid_cnt_ >= MAX_LEVELS) return;
        int i = bid_cnt_++;
        while (i > 0 && bids_[i-1].price() < px) { bids_[i] = bids_[i-1]; --i; }
        bids_[i].id = i; bids_[i].set_price(px); bids_[i].set_qty(qty); bids_[i].ts_ns = 0; bids_[i].flags = 1;
        bid_mask_ |= (1ULL << i);
    }

    void insert_ask(double px, double qty) {
        if (ask_cnt_ >= MAX_LEVELS) return;
        int i = ask_cnt_++;
        while (i > 0 && asks_[i-1].price() > px) { asks_[i] = asks_[i-1]; --i; }
        asks_[i].id = i + MAX_LEVELS; asks_[i].set_price(px); asks_[i].set_qty(qty); asks_[i].ts_ns = 0; asks_[i].flags = 0;
        ask_mask_ |= (1ULL << i);
    }

    int best_bid_level() const { return bid_mask_ ? __builtin_ctzll(bid_mask_) : -1; }
    int best_ask_level() const { return ask_mask_ ? __builtin_ctzll(ask_mask_) : -1; }
    double best_bid() const { int l = best_bid_level(); return l >= 0 ? bids_[l].price() : 0; }
    double best_ask() const { int l = best_ask_level(); return l >= 0 ? asks_[l].price() : 0; }
    double spread() const { double b = best_bid(), a = best_ask(); return (b && a) ? a - b : 0; }

    PackedOrder* bid_orders() { return bids_; }
    PackedOrder* ask_orders() { return asks_; }
    std::size_t bid_count() const { return bid_cnt_; }
    std::size_t ask_count() const { return ask_cnt_; }

private:
    alignas(64) PackedOrder bids_[MAX_LEVELS];
    alignas(64) PackedOrder asks_[MAX_LEVELS];
    alignas(64) uint64_t bid_mask_;
    alignas(64) uint64_t ask_mask_;
    alignas(64) std::size_t bid_cnt_;
    alignas(64) std::size_t ask_cnt_;
};