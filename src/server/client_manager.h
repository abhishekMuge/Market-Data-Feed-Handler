#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H

#include <vector>
#include <cstddef>
#include <mutex>
#include <unordered_set>

struct ClientInfo {
    int fd;
    std::unordered_set<uint16_t> subscriptions; // Fast O(1) lookup per tick
};

class ClientManager {
public:
    ClientManager() = default;
    ~ClientManager();

    // Adds a client and sets low-latency socket options (TCP_NODELAY)
    void add_client(int fd);

    void set_subscription(int fd, const std::vector<uint16_t> &symbols);

    // Sends data to all connected clients; handles disconnections
    void broadcast(const void* data, size_t len);
    // Returns count of active subscribers
    size_t client_count() const;

private:
    std::vector<ClientInfo> clients_;
    mutable std::mutex clients_mutex_; // Thread-safety for connection handling
    
};

#endif