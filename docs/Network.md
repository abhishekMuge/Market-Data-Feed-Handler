### 🏗️ Socket Layer & Framing
The network layer is optimized for **minimal "Time-to-Capture"** using POSIX non-blocking I/O.

#### **1. Low-Latency Configuration**
* **`O_NONBLOCK`:** Sockets are set to non-blocking to prevent the kernel from putting threads to sleep (context switching).
* **`TCP_NODELAY`:** Nagle’s Algorithm is disabled to ensure immediate delivery of small (49-byte) market packets.

#### **2. The "Staging-Slide" Mechanism**
Since TCP is a stream protocol, the client handles packet fragmentation without extra memory allocation:
1.  **Linear Read:** The parser processes complete 49-byte messages in a loop.
2.  **Fragment Handling:** Any "leftover" bytes (less than 49) are shifted to the front of the buffer using `std::memmove`.
3.  **Alignment:** The next `recv()` starts writing immediately after the leftover fragment, perfectly reconstructing the message boundaries.

#### **3. Zero-Copy Pipeline**
* **Kernel to User:** Data moves once from the kernel buffer to the user staging buffer.
* **Direct Access:** The `BinaryParser` treats staging memory as the `MarketMessage` struct directly. **No intermediate objects are created.**

---
