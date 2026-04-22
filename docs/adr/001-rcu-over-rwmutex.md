# ADR 001: RCU over std::shared_mutex for lock-free reads

## Context
MemDB requires high read throughput with low latency, as reads are the majority of operations in KV stores. Writers must not block readers.

## Decision
Use Read-Copy-Update (RCU) instead of std::shared_mutex.

## Rationale
- RCU readers take atomic snapshot with no lock contention
- std::shared_mutex readers acquire shared lock, causing cache line contention under high concurrency
- Benchmark: RCU 6.7x faster than shared_mutex under 16 readers

## Consequences
- Writes are slower due to copying the map
- Acceptable as KV stores are read-heavy
