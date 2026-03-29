#ifndef MARKET_DATA_SOCKET_H
#define MARKET_DATA_SOCKET_H

#include <string>
#include <vector>
#include <cstdint>
#include <sys/epoll.h>

class MarketDataSocket {
public:
    MarketDataSocket();
    ~MarketDataSocket();

    bool connect(const std::string& host, uint16_t port, uint32_t timeout_ms = 5000);
    ssize_t receive(void* buffer, size_t max_len);
    bool send_subscription(const std::vector<uint16_t>& symbol_ids);
    
    bool is_connected() const { return connected_; }
    void disconnect();

    // Low Latency Options
    bool set_tcp_nodelay(bool enable);
    bool set_recv_buffer_size(size_t bytes);
    bool set_socket_priority(int priority);

private:
    int fd_ = -1;
    int epoll_fd_ = -1;
    bool connected_ = false;
    
    // Internal helper to set O_NONBLOCK
    bool set_nonblocking(int fd);
};

#endif