// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/core/tools/contextpool.hpp"
#include "slideio/core/tools/readcontext.hpp"
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace
{
    class CountingContext : public slideio::ReadContext
    {
    public:
        explicit CountingContext(std::atomic<int>& liveCount) : m_liveCount(liveCount) {
            ++m_liveCount;
        }
        ~CountingContext() override { --m_liveCount; }
        int payload = 0;
    private:
        std::atomic<int>& m_liveCount;
    };
}

TEST(ContextPool, defaultMaxIsBoundedAndPositive) {
    const int max = slideio::ContextPool::defaultMax();
    EXPECT_GE(max, 1);
    EXPECT_LE(max, 8);
}

TEST(ContextPool, constructsLazily) {
    std::atomic<int> live{0};
    slideio::ContextPool pool([&live]() {
        return std::make_unique<CountingContext>(live);
    });
    EXPECT_EQ(live.load(), 0) << "no context should exist before the first acquire";
    {
        auto borrow = pool.acquire();
        EXPECT_EQ(live.load(), 1);
    }
    EXPECT_EQ(live.load(), 1) << "a returned context is kept for reuse, not destroyed";
    EXPECT_EQ(pool.contextCount(), 1);
}

TEST(ContextPool, reusesAReturnedContext) {
    std::atomic<int> live{0};
    slideio::ContextPool pool([&live]() {
        return std::make_unique<CountingContext>(live);
    });
    {
        auto borrow = pool.acquire();
        borrow.as<CountingContext>().payload = 42;
    }
    {
        auto borrow = pool.acquire();
        EXPECT_EQ(borrow.as<CountingContext>().payload, 42);
    }
    EXPECT_EQ(pool.contextCount(), 1);
}

TEST(ContextPool, handsOutDistinctContextsToSimultaneousBorrowers) {
    std::atomic<int> live{0};
    slideio::ContextPool pool([&live]() {
        return std::make_unique<CountingContext>(live);
    }, 4);
    auto first = pool.acquire();
    auto second = pool.acquire();
    EXPECT_NE(&first.get(), &second.get());
    EXPECT_EQ(live.load(), 2);
}

TEST(ContextPool, destroysEveryContextOnPoolDestruction) {
    std::atomic<int> live{0};
    {
        slideio::ContextPool pool([&live]() {
            return std::make_unique<CountingContext>(live);
        }, 4);
        auto first = pool.acquire();
        auto second = pool.acquire();
        EXPECT_EQ(live.load(), 2);
    }
    EXPECT_EQ(live.load(), 0);
}

// The bound is the point: a pool of 2 must never construct a third context, no
// matter how many threads ask.
TEST(ContextPool, neverExceedsItsBound) {
    std::atomic<int> live{0};
    std::atomic<int> peak{0};
    slideio::ContextPool pool([&live, &peak]() {
        auto context = std::make_unique<CountingContext>(live);
        int current = live.load();
        int seen = peak.load();
        while (current > seen && !peak.compare_exchange_weak(seen, current)) {
        }
        return context;
    }, 2);

    std::vector<std::thread> threads;
    for (int t = 0; t < 16; ++t) {
        threads.emplace_back([&pool]() {
            for (int i = 0; i < 100; ++i) {
                auto borrow = pool.acquire();
                borrow.as<CountingContext>().payload += 1;
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    EXPECT_LE(peak.load(), 2);
    EXPECT_LE(pool.contextCount(), 2);
}

// neverExceedsItsBound above uses a near-instant factory (make_unique), so
// every construction finishes before a second acquire() reaches the mayGrow
// check -- it cannot observe a bound check that only counts fully-built
// contexts (m_contexts.size()) instead of built-or-promised ones
// (m_borrowed + m_free.size()), because by the time any other thread looks,
// the previous grower has already finished and pushed its context. That gap
// is exactly the bug this pool once had: gating growth on m_contexts.size()
// let every racing thread see "nothing has grown yet" and all of them grow
// at once. This test widens the window with a short, deliberate hold inside
// the factory so concurrent construction is actually observed. Without the
// hold, this test would pass even against the buggy check -- it is not
// testing the same thing as neverExceedsItsBound, it is testing the thing
// that test's fast factory cannot expose.
TEST(ContextPool, neverExceedsItsBoundUnderContention) {
    std::atomic<int> live{0};
    std::atomic<int> concurrentFactoryCalls{0};
    std::atomic<int> peak{0};
    slideio::ContextPool pool([&live, &concurrentFactoryCalls, &peak]() {
        const int current = ++concurrentFactoryCalls;
        int seen = peak.load();
        while (current > seen && !peak.compare_exchange_weak(seen, current)) {
        }
        // The hold: long enough that, with 16 threads racing acquire() and a
        // bound of 2, multiple factory calls are reliably in flight at once
        // -- short enough that the whole test still finishes in well under
        // a second.
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        --concurrentFactoryCalls;
        return std::make_unique<CountingContext>(live);
    }, 2);

    std::vector<std::thread> threads;
    for (int t = 0; t < 16; ++t) {
        threads.emplace_back([&pool]() {
            auto borrow = pool.acquire();
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    EXPECT_LE(peak.load(), 2);
    EXPECT_LE(pool.contextCount(), 2);
}

// No two borrowers may hold the same context at the same time -- the property
// that makes a borrowed TIFF handle safe to use without further locking.
TEST(ContextPool, neverHandsOneContextToTwoBorrowersAtOnce) {
    std::atomic<int> live{0};
    slideio::ContextPool pool([&live]() {
        return std::make_unique<CountingContext>(live);
    }, 4);

    std::atomic<int> collisions{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 16; ++t) {
        threads.emplace_back([&pool, &collisions]() {
            for (int i = 0; i < 200; ++i) {
                auto borrow = pool.acquire();
                CountingContext& context = borrow.as<CountingContext>();
                // Exclusive access means this increment-then-check cannot observe
                // another borrower's value.
                context.payload = 1;
                if (context.payload != 1) {
                    ++collisions;
                }
                context.payload = 0;
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    EXPECT_EQ(collisions.load(), 0);
}

TEST(ContextPool, unboundedGrowsWithBorrowers) {
    std::atomic<int> live{0};
    slideio::ContextPool pool([&live]() {
        return std::make_unique<CountingContext>(live);
    }, slideio::ContextPool::kUnbounded);
    auto a = pool.acquire();
    auto b = pool.acquire();
    auto c = pool.acquire();
    EXPECT_EQ(live.load(), 3);
}

TEST(ContextPool, borrowIsMovable) {
    std::atomic<int> live{0};
    slideio::ContextPool pool([&live]() {
        return std::make_unique<CountingContext>(live);
    });
    auto borrow = pool.acquire();
    {
        slideio::ReadContext* const address = &borrow.get();
        auto moved = std::move(borrow);
        EXPECT_EQ(&moved.get(), address);
    }
    // The moved-from borrow must not have returned the context a second time.
    EXPECT_EQ(pool.contextCount(), 1);
    auto again = pool.acquire();
    EXPECT_EQ(pool.contextCount(), 1) << "double release would have grown the pool";
}
