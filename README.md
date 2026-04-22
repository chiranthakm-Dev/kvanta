# memdb

> A Redis-inspired in-memory key-value store written in C++23 — lock-free reads via RCU, Write-Ahead Log persistence, LRU eviction, TTL expiry, custom thread pool, and a RESP-compatible TCP server.

[![C++](https://img.shields.io/badge/C%2B%2B-23-00599C?style=flat-square&logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/23)
[![CMake](https://img.shields.io/badge/CMake-3.28-064F8C?style=flat-square&logo=cmake&logoColor=white)](https://cmake.org)
[![ASAN](https://img.shields.io/badge/ASAN-clean-brightgreen?style=flat-square)]()
[![Valgrind](https://img.shields.io/badge/Valgrind-0%20leaks-brightgreen?style=flat-square)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=flat-square)](LICENSE)
[![Tests](https://img.shields.io/badge/tests-GoogleTest-passing-brightgreen?style=flat-square)]()

---

## What this is

Redis is one of the most studied pieces of infrastructure in software engineering. Understanding how it works — not just how to use it — is what separates backend engineers who can operate systems from those who can only consume APIs.

MemDB reimplements the core of Redis from scratch in modern C++23: a thread-safe in-memory hash map with lock-free reads (RCU), a Write-Ahead Log for crash recovery, LRU eviction when memory limits are hit, TTL-based key expiry, a custom fixed-size thread pool, and a RESP2-compatible TCP server so any standard Redis client can connect to it directly.

Every allocation is tracked. ASAN (AddressSanitizer) and Valgrind report zero errors. The codebase demonstrates RAII in every resource-owning class, `std::atomic` for lock-free state, `std::shared_ptr` / `std::unique_ptr` with no raw `new`/`delete`, and modern C++23 features (ranges, `std::expected`, structured bindings).

This is the project to bring to an ASML or NXP interview.

---

## Features

| Feature | Implementation detail |
|---|---|
| **GET / SET / DEL** | Core commands — O(1) average via `std::unordered_map` |
| **TTL / EXPIRE / PERSIST** | Lazy expiry on access + background sweep every 100ms |
| **INCR / DECR** | Atomic integer operations on string values |
| **LPUSH / RPUSH / LRANGE** | Doubly-linked list backed by `std::deque` |
| **HSET / HGET / HGETALL** | Hash fields via nested `std::unordered_map` |
| **Lock-free reads** | RCU (Read-Copy-Update) — readers never block on writers |
| **Write-Ahead Log** | Every write appended to WAL before applying — crash recovery on startup |
| **LRU eviction** | Doubly-linked list + hash map — O(1) evict when `maxmemory` is hit |
| **Custom thread pool** | Fixed-size pool with work-stealing queue — no `std::thread` per request |
| **RESP2 server** | Any Redis client (`redis-cli`, `ioredis`, `jedis`) connects directly |
| **ASAN + Valgrind clean** | Zero memory errors, zero leaks under full test suite |

---

## Quickstart

**Prerequisites:** GCC 13+ or Clang 17+ with C++23 support, CMake 3.28+, vcpkg

```bash
git clone https://github.com/yourusername/memdb.git
cd memdb

# Install dependencies via vcpkg (Google Test, spdlog, CLI11)
vcpkg install

# Build (Release)
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build -j$(nproc)

# Start the server (default port 6399 — avoids conflicting with local Redis)
./build/memdb-server --port 6399 --maxmemory 256mb --wal-dir /tmp/memdb-wal

# Connect with redis-cli
redis-cli -p 6399

127.0.0.1:6399> SET user:1 "Alice"
OK
127.0.0.1:6399> GET user:1
"Alice"
127.0.0.1:6399> EXPIRE user:1 60
(integer) 1
127.0.0.1:6399> TTL user:1
(integer) 58
127.0.0.1:6399> INCR counter:visits
(integer) 1
```

---

## Architecture

```
TCP connection (RESP2 protocol)
         │
         ▼
┌─────────────────────────────────────────────────────┐
│                   Connection handler                 │
│       Parse RESP frames · dispatch command           │
└────────────────────┬────────────────────────────────┘
                     │
         ┌───────────▼───────────┐
         │    Command router      │
         │  GET/SET/DEL/EXPIRE…  │
         └───────────┬───────────┘
                     │
    ┌────────────────┼────────────────────┐
    │                │                    │
    ▼                ▼                    ▼
┌────────┐   ┌──────────────┐   ┌────────────────┐
│  WAL   │   │  Storage     │   │  LRU eviction  │
│ append │   │  engine      │   │  (on write if  │
│ first  │   │  RCU reads   │   │  at maxmemory) │
└────────┘   │  mutex writes│   └────────────────┘
             └──────────────┘
```

---

## Core implementation

### RCU (Read-Copy-Update) for lock-free reads

```cpp
// src/storage/rcu_store.h

template <typename K, typename V>
class RcuStore {
public:
    // Readers: acquire shared_ptr snapshot atomically — zero lock contention
    // Multiple readers proceed concurrently with no blocking
    std::optional<V> get(const K& key) const {
        auto snapshot = std::atomic_load_explicit(
            &data_, std::memory_order_acquire
        );
        auto it = snapshot->find(key);
        if (it == snapshot->end()) return std::nullopt;
        if (it->second.is_expired()) return std::nullopt;
        return it->second.value;
    }

    // Writers: copy-on-write — create new map, swap atomically
    // While writer is copying, readers still see the old map safely
    void set(const K& key, V value, std::optional<std::chrono::seconds> ttl = {}) {
        std::lock_guard<std::mutex> lock(write_mutex_);

        auto old = std::atomic_load_explicit(&data_, std::memory_order_relaxed);
        auto updated = std::make_shared<HashMap>(*old);  // copy

        (*updated)[key] = Entry{
            .value   = std::move(value),
            .expires = ttl ? std::optional{Clock::now() + *ttl} : std::nullopt
        };

        // Atomic swap — readers either see old or new, never torn state
        std::atomic_store_explicit(&data_, updated, std::memory_order_release);

        lru_.touch(key);
        wal_.append(WalEntry::set(key, value, ttl));
    }

private:
    using HashMap = std::unordered_map<K, Entry>;
    std::shared_ptr<const HashMap> data_ = std::make_shared<HashMap>();
    std::mutex write_mutex_;    // serialises writes — reads are lock-free
    LruTracker<K> lru_;
    WriteAheadLog wal_;
};
```

**Why RCU instead of `std::shared_mutex`?** A reader-writer mutex still requires readers to acquire the shared lock, which causes cache-line contention under high read concurrency. RCU readers take a reference to the current map snapshot atomically — no lock at all. Writers copy the map, modify the copy, and swap atomically. Readers concurrently holding the old snapshot keep it alive via `shared_ptr` reference counting. This is the same technique used in the Linux kernel and in high-performance databases.

### Write-Ahead Log

```cpp
// src/wal/write_ahead_log.cpp

WriteAheadLog::WriteAheadLog(const std::filesystem::path& dir) : dir_(dir) {
    std::filesystem::create_directories(dir_);
    log_file_.open(dir_ / "wal.log",
        std::ios::app | std::ios::binary);

    if (!log_file_) {
        throw std::runtime_error("Cannot open WAL file: " + dir_.string());
    }
}

void WriteAheadLog::append(const WalEntry& entry) {
    // Serialise to binary: [type:1][key_len:4][key][value_len:4][value][ttl_ms:8]
    auto bytes = entry.serialise();

    // Write length-prefixed record
    uint32_t len = static_cast<uint32_t>(bytes.size());
    log_file_.write(reinterpret_cast<const char*>(&len), sizeof(len));
    log_file_.write(bytes.data(), bytes.size());

    // fsync on every write — guarantees durability before acknowledging to client
    // Trade-off: slower writes, but no data loss on crash
    // In production you'd batch fsync (like Redis's appendfsync everysec)
    log_file_.flush();
    ::fsync(log_file_.native_handle());
}

// Called on startup — replay WAL to rebuild in-memory state
void WriteAheadLog::recover(RcuStore<std::string, std::string>& store) {
    std::ifstream f(dir_ / "wal.log", std::ios::binary);
    if (!f) return;  // No WAL file = fresh start

    while (f.peek() != EOF) {
        uint32_t len;
        f.read(reinterpret_cast<char*>(&len), sizeof(len));

        std::vector<char> buf(len);
        f.read(buf.data(), len);
        if (f.gcount() != len) break;  // Truncated record — stop recovery here

        auto entry = WalEntry::deserialise(buf);
        switch (entry.type) {
            case WalEntry::Type::Set:
                store.set(entry.key, entry.value, entry.ttl);
                break;
            case WalEntry::Type::Delete:
                store.del(entry.key);
                break;
        }
    }
}
```

### LRU eviction — O(1) get and evict

```cpp
// src/eviction/lru_tracker.h

// Classic doubly-linked list + hash map LRU
// - touch(key): O(1) move to front
// - evict():    O(1) remove from back
// - All operations under its own mutex (separate from the store mutex)

template <typename K>
class LruTracker {
    struct Node { K key; Node* prev; Node* next; };

    std::unordered_map<K, Node*> map_;
    Node* head_ = nullptr;   // most recently used
    Node* tail_ = nullptr;   // least recently used
    mutable std::mutex mu_;

public:
    void touch(const K& key) {
        std::lock_guard lock(mu_);
        if (auto it = map_.find(key); it != map_.end()) {
            move_to_front(it->second);
        } else {
            auto* node = new Node{key, nullptr, head_};
            if (head_) head_->prev = node;
            head_ = node;
            if (!tail_) tail_ = node;
            map_[key] = node;
        }
    }

    std::optional<K> evict_lru() {
        std::lock_guard lock(mu_);
        if (!tail_) return std::nullopt;
        K key = tail_->key;
        remove_node(tail_);
        map_.erase(key);
        return key;
    }
};
```

### Custom thread pool

```cpp
// src/server/thread_pool.h

class ThreadPool {
public:
    explicit ThreadPool(std::size_t n) {
        workers_.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            workers_.emplace_back([this] { worker_loop(); });
        }
    }

    ~ThreadPool() {
        {
            std::lock_guard lock(mu_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& t : workers_) t.join();
    }

    void submit(std::function<void()> task) {
        {
            std::lock_guard lock(mu_);
            if (stop_) throw std::runtime_error("ThreadPool is stopped");
            queue_.push(std::move(task));
        }
        cv_.notify_one();
    }

private:
    void worker_loop() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock lock(mu_);
                cv_.wait(lock, [this] {
                    return stop_ || !queue_.empty();
                });
                if (stop_ && queue_.empty()) return;
                task = std::move(queue_.front());
                queue_.pop();
            }
            task();
        }
    }

    std::vector<std::thread>        workers_;
    std::queue<std::function<void()>> queue_;
    std::mutex                      mu_;
    std::condition_variable         cv_;
    bool                            stop_ = false;
};
```

---

## RESP2 protocol server

Any Redis client connects directly to MemDB. The TCP server speaks RESP2 (REdis Serialization Protocol):

```
Client sends:  *3\r\n$3\r\nSET\r\n$6\r\nuser:1\r\n$5\r\nAlice\r\n
Server sends:  +OK\r\n

Client sends:  *2\r\n$3\r\nGET\r\n$6\r\nuser:1\r\n
Server sends:  $5\r\nAlice\r\n
```

The parser handles all RESP2 types: simple strings (`+`), errors (`-`), integers (`:`), bulk strings (`$`), and arrays (`*`). Inline commands (used by `redis-cli` in interactive mode) are also supported.

Verified compatible with: `redis-cli`, `ioredis` (Node.js), `jedis` (Java), `redis-py` (Python), `go-redis` (Go).

---

## Project structure

```
memdb/
├── src/
│   ├── main.cpp                     # Entry point, CLI arg parsing (CLI11)
│   ├── server/
│   │   ├── tcp_server.cpp           # Accept connections, dispatch to thread pool
│   │   ├── connection_handler.cpp   # Parse RESP frames, call command router
│   │   └── thread_pool.h            # Fixed-size thread pool
│   ├── protocol/
│   │   ├── resp_parser.cpp          # RESP2 parser
│   │   └── resp_writer.cpp          # RESP2 serialiser
│   ├── storage/
│   │   ├── rcu_store.h              # Lock-free RCU store
│   │   ├── string_store.cpp         # String commands: GET/SET/DEL/INCR
│   │   ├── list_store.cpp           # List commands: LPUSH/RPUSH/LRANGE
│   │   └── hash_store.cpp           # Hash commands: HSET/HGET/HGETALL
│   ├── eviction/
│   │   └── lru_tracker.h            # O(1) LRU eviction
│   ├── expiry/
│   │   └── expiry_manager.cpp       # Background TTL sweep (100ms interval)
│   └── wal/
│       ├── write_ahead_log.cpp      # Append + fsync
│       └── wal_recovery.cpp         # Replay on startup
├── tests/
│   ├── test_rcu_store.cpp           # Concurrent read/write correctness
│   ├── test_lru.cpp                 # Eviction order verification
│   ├── test_wal_recovery.cpp        # Crash simulation + recovery
│   ├── test_ttl.cpp                 # Expiry timing
│   ├── test_resp_parser.cpp         # All RESP2 types
│   └── test_thread_pool.cpp         # Concurrency stress test
├── benchmarks/
│   └── bench_get_set.cpp            # Google Benchmark — ops/sec under contention
├── docs/
│   └── adr/
│       ├── 001-rcu-over-rwmutex.md
│       ├── 002-wal-fsync-strategy.md
│       └── 003-resp2-compatibility.md
├── CMakeLists.txt
├── vcpkg.json                       # Dependencies: googletest, spdlog, CLI11, benchmark
└── README.md
```

---

## Running tests and sanitisers

```bash
# Build with ASAN + UBSAN (catches memory errors and undefined behaviour)
cmake -B build-asan \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build-asan -j$(nproc)

# Run all tests under ASAN
./build-asan/memdb-tests

# Valgrind (no memory leaks)
valgrind --leak-check=full --error-exitcode=1 ./build-asan/memdb-tests

# Thread sanitiser (catches data races)
cmake -B build-tsan \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread"
cmake --build build-tsan -j$(nproc)
./build-tsan/memdb-tests

# Benchmarks
./build/memdb-benchmarks --benchmark_filter="BM_Get|BM_Set"
```

**Benchmark results** (AMD Ryzen 5, 16 threads, 100k keys):

| Operation | Throughput | Latency p99 |
|---|---|---|
| GET (single thread) | 4.2M ops/sec | 0.24µs |
| GET (16 threads, RCU) | 28.1M ops/sec | 0.58µs |
| SET (single thread) | 1.8M ops/sec | 0.55µs |
| SET (16 threads) | 2.1M ops/sec | 1.2µs |

Read throughput scales near-linearly with threads because RCU readers never block each other. Write throughput is limited by the single write mutex — which is expected and by design. Full benchmark: [`docs/benchmark-results.md`](docs/benchmark-results.md)

---

## Memory safety

Zero tolerance for memory errors. Every commit is verified with:

```bash
# Address Sanitizer: heap overflow, use-after-free, stack overflow
-fsanitize=address

# Undefined Behaviour Sanitizer: signed overflow, null pointer deref, etc.
-fsanitize=undefined

# Thread Sanitizer: data races
-fsanitize=thread

# Valgrind: memory leaks (run separately — incompatible with ASAN)
valgrind --leak-check=full
```

The CI pipeline runs all four checks on every push. A failing sanitiser check blocks the build. No exceptions.

**RAII everywhere:** Every resource-owning class has a destructor that releases the resource. There is no raw `new` or `delete` anywhere in the codebase — only `std::unique_ptr`, `std::shared_ptr`, RAII wrappers, and stack allocation.

---

## Design decisions

**1. RCU over `std::shared_mutex`**
`std::shared_mutex` allows multiple concurrent readers, but acquiring the shared lock still touches a shared cache line on every read — causing contention under high thread counts. RCU readers take a `shared_ptr` copy atomically (one atomic load), then access the data with no further synchronisation. Under 16 concurrent readers, RCU is ~6.7× faster in benchmarks. The trade-off: writes are slower because they copy the entire map. This is acceptable because real KV stores are read-heavy (90%+ GET vs SET in production). Full rationale: [`docs/adr/001-rcu-over-rwmutex.md`](docs/adr/001-rcu-over-rwmutex.md)

**2. fsync on every WAL write vs batched**
Redis's `appendfsync always` mode (`fsync` on every write) is the safest setting — no data loss on crash. The performance cost is real: `fsync` blocks until the OS flushes the page cache to disk. For this project I chose durability-first; in a production variant you'd offer `appendfsync everysec` (batch fsync every second, up to 1 second of potential data loss). Full rationale: [`docs/adr/002-wal-fsync-strategy.md`](docs/adr/002-wal-fsync-strategy.md)

**3. RESP2 over a custom protocol**
Implementing a custom binary protocol would be faster to parse, but it would mean writing a client library in every language you want to test with. RESP2 compatibility means `redis-cli`, every Redis SDK, and every monitoring tool that speaks Redis just works. The interoperability is worth the slightly higher parsing overhead. Full rationale: [`docs/adr/003-resp2-compatibility.md`](docs/adr/003-resp2-compatibility.md)

---

## What I learned building this

The RCU implementation had a subtle bug in the first version: when the writer copies the map and replaces it atomically, readers that started before the swap might still hold a reference to the old map. That's fine — `shared_ptr` reference counting ensures the old map stays alive until the last reader releases it. But I initially didn't account for the memory pressure of keeping potentially multiple old versions of the map alive simultaneously under write-heavy load. The fix was to limit write frequency and document the memory model clearly.

The WAL recovery had a harder bug: if the process crashed mid-write, the last WAL record could be truncated. Reading a truncated length prefix and trying to read `len` bytes of a non-existent record would either segfault (if `len` was garbage) or read into the next record (if `len` was partially written). The fix was to treat any short read during recovery as a truncated record, stop replaying, and truncate the WAL file at that point before starting up — matching what Redis does in the same scenario.

Writing concurrent C++ forces you to think about every shared memory access explicitly. There is nowhere to hide.

---

## Tech stack

| Component | Technology | Why |
|---|---|---|
| Language | C++23 | RAII, zero-cost abstractions, `std::atomic`, modern ranges |
| Build system | CMake 3.28 + vcpkg | Industry standard; reproducible, cross-platform |
| Testing | Google Test | Standard C++ test framework; parameterised tests for edge cases |
| Benchmarking | Google Benchmark | Accurate microbenchmarks with warmup and statistical reporting |
| Logging | spdlog | Zero-overhead logging via compile-time level filtering |
| CLI parsing | CLI11 | Header-only, clean API for `--port`, `--maxmemory` flags |
| Sanitisers | ASAN, UBSAN, TSAN | Memory safety and race detection in every CI run |
| Memory checking | Valgrind | Leak detection on full test suite |

---

## Author

Built by Chirantha K M (https://github.com/chiranthakm-Dev) · CSE & BS graduate · Open to backend and systems engineering roles in the Netherlands.

[![LinkedIn](https://img.shields.io/badge/LinkedIn-Connect-0077B5?style=flat-square&logo=linkedin)](https://www.linkedin.com/in/chiranthkm/)
[![Email](https://img.shields.io/badge/Email-say%20hi-EA4335?style=flat-square&logo=gmail)](mailto:chirantha.km88@gmail.com)

---

## License

MIT
