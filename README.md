
# High-Frequency Trading (HFT) Market Data Pipeline

A ultra-low latency C++20 market data handler capable of processing over **500,000 messages per second** with sub-microsecond wire-to-cache latency. 

This project simulates a real-world exchange environment (Exchange Simulator) and a high-performance subscriber (HFT Client) using advanced systems programming techniques.

## 🚀 Key Technical Features

### 1. Zero-Copy Binary Parsing
To eliminate the overhead of memory allocation and `memcpy` on the hot path, the parser uses **Pointer Casting**. Incoming TCP byte streams are mapped directly onto protocol structures (`reinterpret_cast`), reducing CPU cycles per message to the absolute minimum.

### 2. Lock-Free Symbol Cache
The market data cache uses **Atomic Versioning** and **Cache-Line Alignment**. 
* **Concurrency:** Readers (UI/Strategies) and Writers (Network Thread) can access data simultaneously without Mutex locks.
* **Performance:** `alignas(64)` prevents "False Sharing," ensuring that updates to one symbol do not invalidate the CPU cache for another.

### 3. High-Precision Latency Tracking
Includes a dedicated `LatencyTracker` with:
* **Atomic Ring Buffer:** Stores 1 million raw samples with `< 30ns` recording overhead.
* **Approximate Histogram:** Calculates P50, P95, P99, and P99.9 percentiles using 10ns-resolution buckets.

### 4. Robust Stream Synchronization
The `BinaryParser` implements a "Staging-Slide" mechanism. It handles TCP fragmentation by shifting partial message fragments to the front of the buffer, ensuring the stream never loses alignment even during high-burst volatility.

---

## 🏗️ Project Structure

```text
├── include/
│   ├── common/      # Shared Protocol and Latency structures
│   ├── server/      # Exchange Simulator & Tick Generation logic
│   └── client/      # Feed Processor, Parser, and Symbol Cache
├── src/             # Implementation files
├── run.sh           # Automation script (Build/Run/Demo)
└── CMakeLists.txt   # Build configuration
```

---

## 🛠️ Build & Run Instructions

The project includes a `run.sh` script to automate the environment setup.

### Prerequisites
- CMake 3.10+
- GCC/Clang with C++20 support
- Linux environment (Recommended: Ubuntu/NixOS)
- gnome-terminal

### Commands
| Command | Description |
| :--- | :--- |
| `./run.sh build` | Compiles the project and creates the `build/` directory. |
| `./run.sh server` | Launches the Exchange Simulator (Port 8880). |
| `./run.sh client` | Launches the HFT Client and starts processing data. |
| `./run.sh demo` | **Recommended:** Launches both in separate windows for a live demo. |
| `./run.sh clean` | Removes build artifacts and generated CSV reports. |

---
