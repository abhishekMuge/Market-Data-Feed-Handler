The **Server** is designed for **Determinism** (consistent tick generation), while the **Client** is designed for **Throughput** (processing the firehose).

---

### 🏗️ Architecture: The Deterministic Tick Engine
The server operates as a **Single-Writer, Multiple-Reader** broadcaster. Its primary goal is to simulate a matching engine's market data feed (Level 1) without introducing artificial jitter.

### 🔄 Data Flow
1.  **Tick Generation (`TickGenerator`):** Uses a uniform distribution to generate price movements for 500 symbols. It attaches a 64-bit nanosecond timestamp at the moment of creation.
2.  **Protocol Encapsulation:** The raw data is packed into a **49-byte fixed-length `MarketMessage`**. 
3.  **Integrity Layer:** A CRC32/XOR checksum is calculated and appended to the message footer.
4.  **I/O Multiplexing:** Uses a non-blocking TCP socket to broadcast the same buffer to all connected clients simultaneously.

### ⚡ Performance Decisions
* **Zero Allocation:** All `MarketMessage` objects are reused or created on the stack to avoid `malloc` overhead during the broadcast loop.
* **Busy-Wait vs. Sleep:** The server can be configured to "Busy-Wait" (100% CPU) for maximum message frequency or "Sleep" to simulate lower-speed environments.

---

### 🏗️ Architecture: The Reactive Feed Handler
The client is split into two distinct planes: the **Data Plane** (Hot Path) and the **Management Plane** (Reporting).

### 🔄 Data Flow (The "Hot Path")
1.  **Ingress (`FeedProcessor`):** A dedicated thread polls the TCP socket. It pulls raw bytes into a **lock-free staging buffer**.
2.  **Streaming Alignment (`BinaryParser`):** * Since TCP is a stream, not a packet protocol, the parser handles "fragmented" messages.
    * It uses **Pointer Aliasing** (`reinterpret_cast`) to read the buffer as a `MarketMessage` struct without copying data.
3.  **Validation:** The checksum is verified. If it fails, the parser initiates a "Sync-Slide" to find the next valid message header.
4.  **State Update (`SymbolCache`):**
    * The validated message is pushed to the cache.
    * **Lock-Free Updates:** Uses atomic versioning. The writer increments a version, updates the data, and increments again.

### 🔄 Data Flow (The "Management Plane")
1.  **Latency Tracking:** Every message arrival triggers a delta calculation: $Latency = Time_{Now} - Time_{Server}$.
2.  **Snapshotting:** The `main` thread periodically reads from the `SymbolCache`. Because of atomic versioning, the `main` thread never blocks the `FeedProcessor` thread.

### ⚡ Performance Decisions
* **Thread Pinning:** The `FeedProcessor` thread is designed to be pinned to a specific CPU core to minimize L3 cache misses.
* **Cache-Line Padding:** The `SymbolState` struct is padded to 64 bytes (`alignas(64)`) to prevent **False Sharing**, a common bottleneck in high-frequency systems.

---

### 📊 System Interaction Diagram

```text
[ EXCHANGE SERVER ]                       [ HFT CLIENT ]
       |                                         |
 (Tick Generation)                               |
       |                                         |
 (TCP Broadcast)  -------------------------> (TCP Ingress)
       |                                         |
       |                                  (Binary Parsing)
       |                                         |
       |                          /--------------+--------------\
       |                          |                             |
       |                  (Latency Tracking)            (Symbol Cache)
       |                          |                             |
       |                  [ CSV Histogram ]             [ UI / Strategy ]
```
