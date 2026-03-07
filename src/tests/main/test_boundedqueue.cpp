// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/core/tools/boundedqueue.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

TEST(BoundedQueueTest, BasicPushPop) {
    BoundedQueue<int> queue(10);
    queue.push(42);
    auto result = queue.pop();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

TEST(BoundedQueueTest, MultiplePushPop) {
    BoundedQueue<int> queue(10);
    for (int i = 0; i < 5; ++i) {
        queue.push(i);
    }
    for (int i = 0; i < 5; ++i) {
        auto result = queue.pop();
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), i);
    }
}

TEST(BoundedQueueTest, FIFOOrder) {
    BoundedQueue<std::string> queue(5);
    queue.push("first");
    queue.push("second");
    queue.push("third");

    EXPECT_EQ(queue.pop().value(), "first");
    EXPECT_EQ(queue.pop().value(), "second");
    EXPECT_EQ(queue.pop().value(), "third");
}

TEST(BoundedQueueTest, SetDoneUnblocksPopOnEmptyQueue) {
    BoundedQueue<int> queue(10);
    std::atomic<bool> popReturned{false};
    std::optional<int> result;

    std::thread consumer([&]() {
        result = queue.pop();
        popReturned = true;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(popReturned);

    queue.setDone();
    consumer.join();

    EXPECT_TRUE(popReturned);
    EXPECT_FALSE(result.has_value());
}

TEST(BoundedQueueTest, SetDoneAllowsDrainingRemainingItems) {
    BoundedQueue<int> queue(10);
    queue.push(1);
    queue.push(2);
    queue.push(3);

    queue.setDone();

    EXPECT_EQ(queue.pop().value(), 1);
    EXPECT_EQ(queue.pop().value(), 2);
    EXPECT_EQ(queue.pop().value(), 3);
    EXPECT_FALSE(queue.pop().has_value());
}

TEST(BoundedQueueTest, BoundedBehaviorBlocksProducer) {
    BoundedQueue<int> queue(2);
    std::atomic<bool> pushCompleted{false};

    queue.push(1);
    queue.push(2);

    std::thread producer([&]() {
        queue.push(3);
        pushCompleted = true;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(pushCompleted);

    queue.pop();
    producer.join();

    EXPECT_TRUE(pushCompleted);
}

TEST(BoundedQueueTest, ConcurrentProducerConsumer) {
    BoundedQueue<int> queue(5);
    const int itemCount = 100;
    std::atomic<int> consumedSum{0};

    std::thread producer([&]() {
        for (int i = 1; i <= itemCount; ++i) {
            queue.push(i);
        }
        queue.setDone();
    });

    std::thread consumer([&]() {
        while (auto item = queue.pop()) {
            consumedSum += item.value();
        }
    });

    producer.join();
    consumer.join();

    int expectedSum = (itemCount * (itemCount + 1)) / 2;
    EXPECT_EQ(consumedSum.load(), expectedSum);
}

TEST(BoundedQueueTest, MultipleProducersMultipleConsumers) {
    BoundedQueue<int> queue(10);
    const int itemsPerProducer = 50;
    const int numProducers = 3;
    const int numConsumers = 2;
    std::atomic<int> consumedCount{0};
    std::atomic<int> producersDone{0};

    std::vector<std::thread> producers;
    for (int p = 0; p < numProducers; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < itemsPerProducer; ++i) {
                queue.push(p * itemsPerProducer + i);
            }
            if (++producersDone == numProducers) {
                queue.setDone();
            }
        });
    }

    std::vector<std::thread> consumers;
    for (int c = 0; c < numConsumers; ++c) {
        consumers.emplace_back([&]() {
            while (auto item = queue.pop()) {
                consumedCount++;
            }
        });
    }

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    EXPECT_EQ(consumedCount.load(), numProducers * itemsPerProducer);
}

TEST(BoundedQueueTest, MoveOnlyType) {
    BoundedQueue<std::unique_ptr<int>> queue(5);
    queue.push(std::make_unique<int>(42));

    auto result = queue.pop();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result.value(), 42);
}

TEST(BoundedQueueTest, QueueSizeOne) {
    BoundedQueue<int> queue(1);
    queue.push(1);
    EXPECT_EQ(queue.pop().value(), 1);
    queue.push(2);
    EXPECT_EQ(queue.pop().value(), 2);
}
