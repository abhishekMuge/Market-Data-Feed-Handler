#include "client_manager.h"
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>  
#include <unistd.h>
#include <algorithm>
#include <iostream>
#include <protocol.h>

ClientManager::~ClientManager() {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    // Access .fd since clients_ is now a vector of ClientSession
    for (const auto& session : clients_) {
        close(session.fd);
    }
}

void ClientManager::add_client(int fd) {
    int sndbuf = 16 * 1024 * 1024; // 16MB Send Buffer
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

    int nodelay = 1;
    // Disable Nagle's algorithm for immediate packet dispatch
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
    
    std::lock_guard<std::mutex> lock(clients_mutex_);
    // Create a new session with an empty subscription set
    clients_.push_back({fd, {}});
}

void ClientManager::set_subscription(int fd, const std::vector<uint16_t>& symbols) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (auto& session : clients_) {
        if (session.fd == fd) {
            session.subscriptions.clear();
            session.subscriptions.insert(symbols.begin(), symbols.end());
            std::cout << "[SERVER] Subscriptions updated for FD " << fd 
                      << " (" << symbols.size() << " symbols)\n";
            return;
        }
    }
}

void ClientManager::broadcast(const void* data, size_t len) {
    // Cast to access the symbol_id for filtering
    const MarketMessage* msg = reinterpret_cast<const MarketMessage*>(data);
    
    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (auto it = clients_.begin(); it != clients_.end();) {
        
        // 1. FILTERING: Check if client is interested
        // If subscriptions is empty, we assume they haven't sent the 0xFF command yet
        bool interested = it->subscriptions.empty() || 
                          (it->subscriptions.find(msg->symbol_id) != it->subscriptions.end());

        if (!interested) {
            ++it;
            continue; 
        }
        
        // 2. DISPATCH: Send data to the file descriptor
        ssize_t sent = send(it->fd, data, len, MSG_NOSIGNAL);
        
        if (sent < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                // Flow Control: Drop slow consumer to maintain server determinism
                std::cerr << "[WARN] Slow consumer " << it->fd << " dropped (Buffer Full).\n";
                ++it;
                continue;
            } else {
                std::cerr << "[INFO] Client " << it->fd << " disconnected.\n";
                close(it->fd);
                it = clients_.erase(it);
            }
            // close(it->fd);
            // it = clients_.erase(it);
        } else {
            ++it;
        }
    }
}

size_t ClientManager::client_count() const {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    return clients_.size();
}