#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <atomic>
#include <chrono>
#include "server/queue.h"

TEST_CASE("close wakes a blocked consumer"){
    BoundedQueue<int> q(4);

    //Things we are asserting
    std::atomic<bool> started{false};
    std::atomic<bool> returned{false};
    std::atomic<bool> pop_result{true};

    int out = 0;
    //jthread -> auto join 
    std::jthread consumer([&]{ started.store(true, std::memory_order_release);
        bool ok = q.pop(out); //should block till close()
        pop_result.store(ok, std::memory_order_release);
        returned.store(true, std::memory_order_release);
    });
    
    //Wait until thread is running (not perfect, but fine for this test)
    while(!started.load(std::memory_order_acquire)){
        std::this_thread::yield();
    }

    // Give it a moment to reach the wait() (optional but practical)
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    q.close();

    //Wait bounded time for the thread to finish
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while(!returned.load(std::memory_order_acquire) &&
            std::chrono::steady_clock::now() < deadline){
                std::this_thread::yield();
            }
    REQUIRE(returned.load(std::memory_order_acquire) == true);
    REQUIRE(pop_result.load(std::memory_order_acquire) == false);


}