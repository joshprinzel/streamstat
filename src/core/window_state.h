/*
    WindowState invariants

    Time Semantics
    1. Fixed bucket width: The window is partitioned into buckets of width W = bucket_width_ms
    2. Bucket alignment: Any timestamp t belongs to the bucket whose start is start = floor(t/W) * W
    3. Window coverage: At any moment "now", the window represents the interval [now - (N*W) + W, now] in bucket units (i.e., it can hold at most N distinct bucket-start times)

    Ring buffer / ovewrite Semantics
    4. Fixed Storage: The window stores exactly N = num_buckets bucket slots (no growth)
    5. Slot mapping: A bucket with aligned start s maps to index idx = (s/W) % N
    6. Staleness check: A slot is valid for start time s if and only if slot.bucket_start_ms == s.
    7. Overwrite on reuse: If a slot is accessed for a different start time, it is reset (clears stats, sets new bucket_start_ms). Old data is considered expire/overwritten.

    Statistical Correctness
    8. Per-bucket summaries: Each bucket stores per-feature RunningStats that summarize only values observed whose timestamps fall in that bucket's interval.
    9. Window snapshot: Querying window stats returns a snapshot computed by merging the valid buckets' per-feature RunningStats. (This does not mutate the window.)
    10. Single-Write assumption: Observe is called only by the owning shard worker thread. No internal locks required.

    Memory / PMR intent 
    11. Allocator ownership: All internal containers are allocator-aware (std::pmr::*) and use the window's memory resource (ultimately shard pool/scratch).
    12. No per-observation frees: Bucket reuse clears containers; long-lived allocations come from the pool; scratch allocations are reset at batch boundaries (later).
*/



/*
    Observe(feature_id, value, timestamp_ms) Algorithm Pseudo-Code
    For each (feature_id, value, timestamp_ms)
        1. Compute aligned_start = floor(timestamp_ms / W) * W
        2. Compute idx = (aligned_start /W) % N
        3. Bucket& b = buckets[idx]
        4. If b.start_ms != aligned_start -> reset bucket
            b.start = aligned_start
            b.stats.clear()
        5. Update:
            b.stats[feature_idx].Observe(value)
*/