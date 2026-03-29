#include "exchange_simulator.h"
#include "../../includes/common/ring_buffer.h"  // Our new lock-free buffer
#include <sys/epoll.h>
#include <fcntl.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

// Define a buffer size (must be power of 2). 65536 is usually a sweet spot.
RingBuffer<MarketMessage, 65536> dispatch_buffer;

ExchangeSimulator::ExchangeSimulator(uint16_t port, size_t num_symbols)
    : port_(port), num_symbols_(num_symbols), generator_(num_symbols) {}

ExchangeSimulator::~ExchangeSimulator() {
    running_ = false;
    if (listen_fd_ != -1) close(listen_fd_);
    if (epoll_fd_ != -1) close(epoll_fd_);
}

void ExchangeSimulator::set_tick_rate(uint32_t ticks_per_second) {
    tick_rate_hz_ = ticks_per_second;
    std::cout << "[SIM] Tick rate updated to " << ticks_per_second << " Hz\n";
}

void ExchangeSimulator::enable_fault_injection(bool enable) {
    fault_enabled_ = enable;
    std::cout << "[SIM] Fault injection " << (enable ? "enabled" : "disabled") << "\n";
}

void ExchangeSimulator::start() {
    listen_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(listen_fd_, (struct sockaddr*)&addr, sizeof(addr));
    listen(listen_fd_, SOMAXCONN);

    epoll_fd_ = epoll_create1(0);
    epoll_event ev{.events = EPOLLIN, .data = {.fd = listen_fd_}};
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &ev);
    
    running_ = true;
}
void ExchangeSimulator::handle_new_connections() {
    epoll_event events[64];
    // Non-blocking wait for any network activity
    int nfds = epoll_wait(epoll_fd_, events, 64, 0); 

    for (int i = 0; i < nfds; ++i) {
        int fd = events[i].data.fd;

        if (fd == listen_fd_) {
            // CASE 1: New Client Joining
            std::cout << "new client j\n";
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(listen_fd_, (struct sockaddr*)&client_addr, &client_len);
            
            if (client_fd > 0) {
                fcntl(client_fd, F_SETFL, O_NONBLOCK);
                
                clients_.add_client(client_fd);
                
                // Add this new client to epoll so we can hear their subscriptions
                epoll_event ev{.events = EPOLLIN | EPOLLET, .data = {.fd = client_fd}};
                epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev);
                
                std::cout << "[SIM] New client connected on FD " << client_fd << "\n";
            }
        } else if (events[i].events & EPOLLIN) {
            // CASE 2: Existing Client sending 0xFF Subscription
            std::cout << "calling process_client_subscription\n";
            process_client_subscription(fd);
        }
    }
}

void ExchangeSimulator::process_client_subscription(int client_fd) {
    uint8_t buffer[1024]; // Large enough for 500 symbols
    ssize_t bytes = recv(client_fd, buffer, sizeof(buffer), MSG_DONTWAIT);

    if (bytes <= 0) {
        // Handle disconnect if needed (ClientManager already handles send errors)
        return;
    }

    // Protocol Check: Start byte must be 0xFF
    if (buffer[0] == 0xFF && bytes >= 3) {
        uint16_t count;
        std::memcpy(&count, &buffer[1], 2);
        count = ntohs(count); // Convert from Network to Host byte order

        // Validate we have enough bytes for the reported count
        if (bytes >= (3 + count * 2)) {
            std::vector<uint16_t> symbols;
            symbols.reserve(count);

            for (int i = 0; i < count; ++i) {
                uint16_t sym_id;
                std::memcpy(&sym_id, &buffer[3 + (i * 2)], 2);
                // std::cout << "[client]: " << client_fd << "[sub symbol]:" << ntohs(sym_id) << std::endl;
                symbols.push_back(ntohs(sym_id));
            }

            // Update the ClientManager with the new interest list
            clients_.set_subscription(client_fd, symbols);
        }
    }
}
// THE NEW MULTITHREADED RUN
void ExchangeSimulator::run() {
    // Start the Network Dispatcher Thread
    std::thread dispatcher([this]() {
        int count = 0;
        while (running_) {
            MarketMessage msg;
            // Pop from lock-free buffer and send to wire
            if (dispatch_buffer.pop(msg)) {
                msg.checksum = calculate_checksum(msg);
                clients_.broadcast(&msg, sizeof(msg));
                // std::this_thread::sleep_for(std::chrono::milliseconds(100)); // added this to check if work flow is correct.
                if (++count % 100000 == 0) {
                    // Pause for 1 second after every 1k messages
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            } else {
                // If buffer is empty, a short pause or hint helps CPU power
                std::this_thread::yield(); 
            }
        }
    });

    // Main Thread acts as the Generator (Producer)
    auto tick_interval = std::chrono::nanoseconds(1000000000 / tick_rate_hz_);
    auto next_tick = std::chrono::high_resolution_clock::now();

    std::cout << "[SIM] Generator thread spinning at " << tick_rate_hz_ << " Hz\n";

    while (running_) {
        // Handle new client connections (Management overhead)
        handle_new_connections();

        auto now = std::chrono::high_resolution_clock::now();
        if (now >= next_tick) {
            uint32_t sym_idx = rand() % num_symbols_;
            auto msg = generator_.generate(sym_idx, global_seq_++);
            msg.timestamp_ns = now.time_since_epoch().count();

            // Push to lock-free buffer (Busy-wait if buffer is full)
            while (!dispatch_buffer.push(msg) && running_) {
                // In HFT, we might drop the tick if the buffer is full 
                // to maintain "Real-time" status.
            }
            
            next_tick += tick_interval;
        }
    }

    if (dispatcher.joinable()) dispatcher.join();
}