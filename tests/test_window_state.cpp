#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "streamfeat/core/window_state.h"

// WindowState tests
//
// These tests validate the core time-to-bucket mapping, ring overwrite semantics,
// and query-time window bounds. They intentionally test observable behavior only
// (not internal bucket/index details) so implementation can change without
// rewriting tests.

TEST_CASE("values in same bucket accumulate") {
  // Guard: alignment must map multiple timestamps within [0, W) to the same bucket.
  std::pmr::unsynchronized_pool_resource pool;
  WindowState w(/*bucket_width_ms=*/1000, /*num_buckets=*/3, pool);

  // Both timestamps align to bucket start 0ms.
  w.Observe(1, 1.0, 100);
  w.Observe(1, 3.0, 900);

  auto s = w.Aggregate(1, /*now_ms=*/900);
  REQUIRE(s.count() == 2);
  REQUIRE(s.mean() == Catch::Approx(2.0));
}

TEST_CASE("bucket boundary splits correctly") {
  // Guard: boundary behavior uses floor(t/W)*W (t==W is in the next bucket).
  std::pmr::unsynchronized_pool_resource pool;
  WindowState w(/*bucket_width_ms=*/1000, /*num_buckets=*/3, pool);

  // 999ms -> bucket 0ms, 1000ms -> bucket 1000ms
  w.Observe(1, 1.0, 999);
  w.Observe(1, 3.0, 1000);

  auto s = w.Aggregate(1, /*now_ms=*/999);
  REQUIRE(s.count() == 1);
}

TEST_CASE("ring overwrites oldest bucket") {
  // Guard: after advancing past N buckets, the oldest bucket must be dropped.
  // Here: N=3, W=1000ms. We insert 4 distinct bucket intervals.
  std::pmr::unsynchronized_pool_resource pool;
  WindowState w(/*bucket_width_ms=*/1000, /*num_buckets=*/3, pool);

  w.Observe(1, 1.0, 100);   // bucket 0
  w.Observe(1, 1.0, 1100);  // bucket 1000
  w.Observe(1, 1.0, 2100);  // bucket 2000
  w.Observe(1, 1.0, 3100);  // bucket 3000 (overwrites slot that previously held bucket 0)

  auto s = w.Aggregate(1, /*now_ms=*/3100);
  REQUIRE(s.count() == 3);
}

TEST_CASE("query window excludes old buckets") {
  // Guard: Aggregate must apply query-time bounds, not merely "slot validity".
  // At now=4000, aligned end=4000 and window covers [2000, 4000] for N=3,W=1000.
  std::pmr::unsynchronized_pool_resource pool;
  WindowState w(/*bucket_width_ms=*/1000, /*num_buckets=*/3, pool);

  w.Observe(1, 1.0, 0);
  w.Observe(1, 1.0, 1000);

  auto s = w.Aggregate(1, /*now_ms=*/4000);
  REQUIRE(s.count() == 0);
}


TEST_CASE("out-of-order events within are included"){
    // Guard: Observe must not assume timsetamps are monotonic; ordering should not matter
    // as long as events fall within the query window
    std::pmr::unsynchronized_pool_resource pool;
    WindowState w(/*bucket_width_ms=*/1000, /*num_buckets=*/3, pool);

    //Insert newer bucket first, then an older bucket still inside the window.
    w.Observe(1, 10.0, 2100); //bucket 2000
    w.Observe(1, 20.0, 1000) ;//bucket 1000 (late arrival)

    auto s = w.Aggregate(1, /*now_ms=*/ 2100);
    REQUIRE(s.count() == 2);
    REQUIRE(s.mean() == Catch::Approx(15.0));
}

TEST_CASE("late event can overwrite newer bucket if outside retention") {
  std::pmr::unsynchronized_pool_resource pool;
  WindowState w(/*bucket_width_ms=*/1000, /*num_buckets=*/3, pool);

  w.Observe(1, 1.0, 3100); // bucket 3000
  w.Observe(1, 5.0, 100);  // bucket 0 overwrites same ring slot

  // now=3100 => range [1000,3000]; bucket 3000 wiped, bucket 0 excluded
  {
    auto s = w.Aggregate(1, /*now_ms=*/3100);
    REQUIRE(s.count() == 0);
  }

  // now=100 => end=0 => range [-2000,0]; bucket 0 included
  {
    auto s = w.Aggregate(1, /*now_ms=*/100);
    REQUIRE(s.count() == 1);
    REQUIRE(s.mean() == Catch::Approx(5.0));
  }
}
