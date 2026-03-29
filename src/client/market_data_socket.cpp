#include "../../includes/client/market_data_socket.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <iostream>

MarketDataSocket::MarketDataSocket() {
    epoll_fd_ = epoll_create1(0);
}

MarketDataSocket::~MarketDataSocket() {
    disconnect();
    if (epoll_fd_ != -1) close(epoll_fd_);
}

bool MarketDataSocket::connect(const std::string& host, uint16_t port, uint32_t timeout_ms) {
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) return false;

    // 1. Set Low Latency Defaults
    set_tcp_nodelay(true);
    set_recv_buffer_size(16 * 1024 * 1024); // 4MB requirement
    set_nonblocking(fd_);

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr);

    // 2. Non-blocking Connect
    int res = ::connect(fd_, (struct sockaddr*)&server_addr, sizeof(server_addr));
    
    if (res < 0 && errno != EINPROGRESS) {
        close(fd_);
        return false;
    }

    // 3. Epoll for Connection Completion
    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET; // Edge-Triggered
    ev.data.fd = fd_;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd_, &ev);

    epoll_event events[1];
    int nfds = epoll_wait(epoll_fd_, events, 1, timeout_ms);

    if (nfds > 0) {
        int optval;
        socklen_t optlen = sizeof(int);
        getsockopt(fd_, SOL_SOCKET, SO_ERROR, &optval, &optlen);
        if (optval == 0) {
            connected_ = true;
            std::cout << "[CLIENT] Connected to " << host << ":" << port << "\n";
            return true;
        }
    }

    disconnect();
    return false;
}

ssize_t MarketDataSocket::receive(void* buffer, size_t max_len) {
    if (!connected_) return -1;

    ssize_t bytes = recv(fd_, buffer, max_len, MSG_DONTWAIT);
    
    if (bytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        connected_ = false; // Connection dropped
        return -1;
    } else if (bytes == 0) {
        connected_ = false; // Graceful shutdown by server
        return -1;
    }
    
    return bytes;
}

// bool MarketDataSocket::send_subscription(const std::vector<uint16_t>& symbol_ids) {
//     if (!connected_) return false;

//     // Header: 0xFF (1), Count (2), Symbols (2 * N)
//     size_t msg_size = 1 + 2 + (symbol_ids.size() * 2);
//     std::vector<uint8_t> buffer(msg_size);

//     buffer[0] = 0xFF;
//     uint16_t count = htons(static_cast<uint16_t>(symbol_ids.size()));
//     memcpy(&buffer[1], &count, 2);

//     for (size_t i = 0; i < symbol_ids.size(); ++i) {
//         uint16_t id = htons(symbol_ids[i]);
//         memcpy(&buffer[3 + (i * 2)], &id, 2);
//     }

//     return send(fd_, buffer.data(), buffer.size(), 0) > 0;
// }

bool MarketDataSocket::set_tcp_nodelay(bool enable) {
    int opt = enable ? 1 : 0;
    return setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) == 0;
}

bool MarketDataSocket::set_recv_buffer_size(size_t bytes) {
    return setsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &bytes, sizeof(bytes)) == 0;
}

bool MarketDataSocket::set_socket_priority(int priority) {
    return setsockopt(fd_, SOL_SOCKET, SO_PRIORITY, &priority, sizeof(priority)) == 0;
}

bool MarketDataSocket::set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

void MarketDataSocket::disconnect() {
    if (fd_ != -1) {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd_, nullptr);
        close(fd_);
        fd_ = -1;
    }
    connected_ = false;
}
bool MarketDataSocket::send_subscription(const std::vector<uint16_t>& symbol_ids) {
    if (!connected_ || fd_ == -1) return false;

    // 1. Calculate total buffer size:
    // Header (1 byte) + Count (2 bytes) + (N symbols * 2 bytes each)
    uint16_t num_symbols = static_cast<uint16_t>(symbol_ids.size());
    size_t total_size = 1 + 2 + (num_symbols * 2);

    std::vector<uint8_t> buffer(total_size);

    // 2. Set the Header (0xFF)
    buffer[0] = 0xFF;

    // 3. Set the Count (2 bytes, Big-Endian/Network Byte Order)
    uint16_t net_count = htons(num_symbols);
    std::memcpy(&buffer[1], &net_count, 2);

    // 4. Append Symbol IDs (2 bytes each, Big-Endian)
    for (size_t i = 0; i < num_symbols; ++i) {
        uint16_t net_id = htons(symbol_ids[i]);
        // Calculate offset: 1 (header) + 2 (count) + (index * 2)
        std::memcpy(&buffer[3 + (i * 2)], &net_id, 2);
    }

    // 5. Send the raw binary packet
    ssize_t sent = send(fd_, buffer.data(), buffer.size(), MSG_NOSIGNAL);
    
    return (sent == static_cast<ssize_t>(total_size));
}