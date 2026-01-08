#include <catch2/catch_test_macros.hpp>
#include "streamfeat/runtime/queue.h"

TEST_CASE("try_push respects capacity") {
    BoundedQueue<int> q(2);

    REQUIRE(q.try_push(1) == true);
    REQUIRE(q.try_push(2) == true);
    REQUIRE(q.try_push(3) == false);

    REQUIRE(q.size() == 2);
}

TEST_CASE("FIFO ordering single-thread") {
    BoundedQueue<int> q(10);

    REQUIRE(q.try_push(1));
    REQUIRE(q.try_push(2));

    int out = 0;

    REQUIRE(q.pop(out) == true);
    REQUIRE(out == 1);

    REQUIRE(q.pop(out) == true);
    REQUIRE(out == 2);
}

TEST_CASE("close makes pop return false when closed+empty") {
    BoundedQueue<int> q(5);

    q.close();

    int out = 0;
    REQUIRE(q.pop(out) == false);
}

TEST_CASE("close still allows draining existing items") {
    BoundedQueue<int> q(5);

    REQUIRE(q.try_push(1));
    REQUIRE(q.try_push(2));

    q.close();

    int out = 0;
    REQUIRE(q.pop(out) == true); REQUIRE(out == 1);
    REQUIRE(q.pop(out) == true); REQUIRE(out == 2);
    REQUIRE(q.pop(out) == false);
}

TEST_CASE("close is idempotent") {
    BoundedQueue<int> q(5);

    q.close();
    q.close();

    int out = 0;
    REQUIRE(q.pop(out) == false);
}



