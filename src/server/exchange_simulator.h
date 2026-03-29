#ifndef EXCHANGE_SIMULATOR_H
#define EXCHANGE_SIMULATOR_H

#include <atomic>
#include <cstdint>
#include <unordered_map>
#include "tick_generator.h"
#include "client_manager.h"

class ExchangeSimulator {
public:
    ExchangeSimulator(uint16_t port, size_t num_symbols = 100);
    ~ExchangeSimulator();

    void start();
    void run();
    void set_tick_rate(uint32_t ticks_per_second);
    void enable_fault_injection(bool enable);

private:
    void handle_new_connections();

    void process_client_subscription(int client_fd);

    uint16_t port_;
    size_t num_symbols_;
    uint32_t tick_rate_hz_ = 1000;
    std::atomic<bool> running_{false};
    bool fault_enabled_ = false;
    

    int listen_fd_ = -1;
    int epoll_fd_ = -1;
    uint64_t global_seq_ = 1;

    TickGenerator generator_;
    ClientManager clients_;
};

#endif