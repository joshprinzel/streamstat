# System #1: `streamfeat`
## Streaming Feature Statistics & Drift Monitor (C++ / gRPC)

## Overview
`streamfeat` is a **CPU-first, streaming-oriented C++ service** that ingests feature events over **gRPC** and maintains **online, mergeable statistics** per entity and per time window.

It is designed to demonstrate:
- Ownership of a real system (not just a component)
- A **performance-critical hot path**
- Correct handling of **long-lived state**
- Explicit **concurrency** and **memory management** decisions
- A measured, documented **performance improvement**

The core numerical algorithms are provided by **`fastnum`**; this system focuses on **architecture, state management, and performance engineering**.

---

## Goals

### Functional goals
- Ingest streaming feature events
- Maintain online mean/variance (and optional covariance)
- Support short and long rolling time windows
- Detect feature distribution drift
- Expose query APIs for stats and drift

### Non-goals (v1)
- GPU acceleration
- Exactly-once semantics across machines
- Distributed consensus
- Model training or inference

---

## External API (gRPC)

### Feature schema registry (cold path)
Feature names are registered once and mapped to integer IDs to avoid strings in the hot path.

**RPC:** `RegisterSchema`

**Input**
- Repeated feature names (`string`)

**Output**
- Mapping `{feature_name → feature_id (int32)}`

---

### Ingestion (hot path)
**RPC:** `IngestBatch`

Clients may send a single event or a microbatch.

**Event**
- `string entity_id`
- `int64 timestamp_ms`
- Repeated `{int32 feature_id, double value}`

**Reply**
- `accepted_events`
- `dropped_events` (too old / invalid)
- `accepted_feature_values`

Events older than the maximum supported window are dropped.

---

### Queries (read path)

**RPC:** `GetFeatureStats`

Query statistics for one feature.

**Input**
- `entity_id`
- `feature_id`
- Window enum `{SHORT, LONG}`

**Output**
- count
- mean
- population variance
- sample variance

---

### Drift detection
**RPC:** `GetDrift`

Compares short vs long windows.

**Computed metrics**
- Mean shift z-score  
  `abs(mu_short - mu_long) / max(sigma_long, eps)`
- Variance ratio  
  `sigma_short / max(sigma_long, eps)` (and inverse)

**Default thresholds**
- mean z-score > 3.0
- variance ratio > 2.0 (or inverse)
- `eps = 1e-12`

Returns stats, metrics, and boolean trigger flags.

---

## Time Windows

Two rolling windows are supported:

| Window | Duration | Bucket width | Buckets |
|------|---------|--------------|---------|
| Short | 5 min | 5 s | 60 |
| Long  | 1 hr | 5 s | 720 |

Each window is implemented as a **ring buffer of fixed-time buckets**.

Buckets automatically reset when time advances.

---

## Internal Architecture

### Sharded single-writer backend
- Shard count `S` configurable (default: 8)
- Shard chosen by `hash(entity_id) % S`
- Each shard owns:
  - One worker thread
  - A bounded event queue
  - All state for its entities

**Key property:**  
All state mutation occurs on shard workers → **no locks on the update path**.

gRPC threads are concurrent, but backend state is thread-safe by ownership.

---

### Queues & backpressure
- Multi-producer → single-consumer queues
- If a shard queue is full:
  - incoming batch is dropped
  - drop counter is incremented

Overload behavior is explicit and measurable.

---

## Memory Management (Arenas / PMR)

Each shard owns two PMR-based memory resources:

### 1) Scratch arena (transient)
- `std::pmr::monotonic_buffer_resource`
- Reset after each drained microbatch
- Used for temporary vectors and parsing buffers

### 2) Persistent pool (long-lived state)
- `std::pmr::unsynchronized_pool_resource`
- Used for:
  - entity state
  - window structures
  - bucket maps

All containers are allocator-aware (`std::pmr::*`).

---

### Eviction strategy
- Each entity tracks `last_seen_timestamp_ms`
- If `now - last_seen > TTL` (e.g., 2 hours):
  - entity is tombstoned
- Periodically, the shard rebuilds its map into a fresh pool:
  - compacts memory
  - avoids per-object frees

This makes arena allocation practical and predictable.

---

## Core Data Structures

### Shard
```cpp
struct Shard {
  WorkerThread worker;
  Queue<EventBatch> q;

  std::pmr::unsynchronized_pool_resource pool;
  std::pmr::monotonic_buffer_resource scratch;

  std::pmr::unordered_map<EntityKey, EntityState> entities;
};
```

### EntityState
```cpp
	struct EntityState {
	  int64_t last_seen_ms;
	
	  WindowState short_w; // 5 min ring
	  WindowState long_w;  // 1 hr ring
	};

```

### WindowState
```cpp
	struct WindowState {
	  int64_t bucket_width_ms;      // 5000
	  int32_t num_buckets;          // 60 or 720
	  int64_t base_bucket_start_ms; // aligns timestamps to buckets
	
	  std::pmr::vector<Bucket> buckets;
	};

```

### Bucket
```cpp
	struct Bucket {
	  int64_t bucket_start_ms;
	  std::pmr::unordered_map<int32_t, fastnum::RunningStats<double>> stats;
	};
```


## Hot Path: Update Logic

For each event in a shard worker:
1. Compute bucket_idx = (timestamp_ms / bucket_width_ms) % num_buckets
2. Get Bucket& b = buckets\[bucket_idx]
3. If b.bucket_start_ms != aligned_bucket_start:
	- reset bucket (clear map, set new start time)
4. For each (feature_id, value):
	- b.stats\[feature_id].observe(value)
5. Update entity last_seen_ms


This loop is the performance-critical section:
- hash map access
- branch on bucket rotation
- numeric update

It is profiled and optimized

## Query Path

Queries are routed to the owning shard.

Implementation choice (v1):
- Query enqueued as a "task" to the shard worker
- Worker computes result and replies
This preserves single-writer semantics and avoids locks entirely


## Performance Strategy

### Metrics Collected
- ingest throughput (events/sec and feature_values/sec)
- p50 / p99 ingest latency
- per-shared queue depth
- dropped batch count
- memory usage vs entity count

## Required performance win
At least one measured improvement e.g.:
- PMR arenas vs new/delete
- microbatch draining
- integer features IDs vs strings
-  improved state layout / hash map choices

Before/after results documented

## Testing
- Unit tests for:
	- bucket rotation correctness
	- window merge equivalence
	- drift calculation logic
- Load generator for reproducible benchmarks


## Repo Layout
```
	streamfeat/
	├── CMakeLists.txt
	├── proto/streamfeat.proto
	├── src/
	│   ├── main.cpp
	│   ├── server/
	│   ├── core/
	│   └── util/
	├── tests/
	├── bench/
	└── README.md
```

## "Done" criteria
This project is complete when:
- gRPC ingest, query, and drift endpoints work end to end
- memory growth is bounded and explained
- performance metrics are reproducible
- one optimization shows a clear improvement
- documentation explains architecture, tradeoffs, and results


