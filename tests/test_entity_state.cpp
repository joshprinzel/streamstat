#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <memory_resource>
#include "core/entity_state.h"

TEST_CASE("EntityState Observe updates both windows and last_seen"){
    std::pmr::unsynchronized_pool_resource pool;

    EntityState e(pool);
    e.Observe(1,1.0,1000);
    e.Observe(1, 3.0, 500); //out of order, last_seen should stay 1000

    REQUIRE(e.last_seen_ms() == 1000);

    auto s_short = e.short_window().Aggregate(1,1000);
    auto s_long = e.long_window().Aggregate(1,1000);

    REQUIRE(s_short.count() == 2);
    REQUIRE(s_long.count() == 2);
}