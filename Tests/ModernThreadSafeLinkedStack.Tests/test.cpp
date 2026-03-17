#include "pch.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <string>

#include "../../ThreadSafeStack/ModernThreadSafeLinkedStack.h"

/**
 * @category Functional Tests (Single-threaded)
 * @brief These tests verify the core LIFO logic and basic state management
 * in a deterministic, single-threaded environment.
 */

 // @test Verifies that a newly created stack is empty.
TEST(ModernThreadSafeLinkedStackTest, InitialStateIsCorrect) {
    ModernThreadSafeLinkedStack<int> stack;
    EXPECT_TRUE(stack.isEmpty());
}

// @test Validates basic push and pop operations, ensuring LIFO (Last-In, First-Out) order.
TEST(ModernThreadSafeLinkedStackTest, PushAndPopValues) {
    ModernThreadSafeLinkedStack<int> stack;
    stack.push(10);
    stack.push(20);

    EXPECT_FALSE(stack.isEmpty());

    int val = 0;
    // Check pop by reference (should be the last value pushed: 20)
    bool success = stack.tryPop(val);
    EXPECT_TRUE(success);
    EXPECT_EQ(val, 20);

    // Check pop by shared_ptr (should be the first value pushed: 10)
    auto ptr = stack.tryPop();
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, 10);

    EXPECT_TRUE(stack.isEmpty());
}

// @test Tests tryPop variants for empty stack scenarios to ensure no crashes or exceptions.
TEST(ModernThreadSafeLinkedStackTest, TryPopEmptyStackReturnsNullOrFalse) {
    ModernThreadSafeLinkedStack<int> stack;
    int val = -1;

    EXPECT_FALSE(stack.tryPop(val));
    EXPECT_EQ(val, -1); // Value should remain unchanged
    EXPECT_EQ(stack.tryPop(), nullptr);
}

// @test Confirms that clear() removes all elements and the stack becomes empty.
TEST(ModernThreadSafeLinkedStackTest, ClearEmptiesTheStack) {
    ModernThreadSafeLinkedStack<std::string> stack;
    stack.push("Alpha");
    stack.push("Beta");
    stack.push("Gamma");

    stack.clear();
    EXPECT_TRUE(stack.isEmpty());
    EXPECT_EQ(stack.tryPop(), nullptr);
}

// @test Stress test for iterative cleanup logic. 
// Verifies that a very deep stack does not cause a Stack Overflow on destruction.
TEST(ModernThreadSafeLinkedStackTest, HandlesLargeStacksWithoutStackOverflow) {
    {
        ModernThreadSafeLinkedStack<int> stack;
        const int deepStackSize = 100000;
        for (int i = 0; i < deepStackSize; ++i) {
            stack.push(i);
        }
        // Stack goes out of scope here; destructor is called.
    }
    SUCCEED(); // If it didn't crash, the iterative cleanup works.
}

/**
 * @category Concurrency Tests (Multi-threaded)
 * @brief These tests verify thread safety under high contention to detect
 * race conditions or data corruption.
 */

 // @test Verifies thread safety during simultaneous push operations from multiple threads.
TEST(ModernThreadSafeLinkedStackTest, ConcurrentPushes) {
    ModernThreadSafeLinkedStack<int> stack;
    const int numThreads = 8;
    const int pushesPerThread = 2000;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&stack, pushesPerThread]() {
            for (int j = 0; j < pushesPerThread; ++j) {
                stack.push(j);
            }
            });
    }

    for (auto& t : threads) t.join();

    // Verify all items were pushed by popping them and counting
    int totalCount = 0;
    int dummy;
    while (stack.tryPop(dummy)) {
        totalCount++;
    }
    EXPECT_EQ(totalCount, numThreads * pushesPerThread);
}

// @test Ensures no data loss occurs when multiple threads consume the stack simultaneously.
TEST(ModernThreadSafeLinkedStackTest, ConcurrentPops) {
    ModernThreadSafeLinkedStack<int> stack;
    const int totalElements = 10000;
    for (int i = 0; i < totalElements; ++i) stack.push(i);

    std::atomic<int> popCount{ 0 };
    const int numThreads = 10;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&stack, &popCount]() {
            int val;
            while (stack.tryPop(val)) {
                popCount++;
            }
            });
    }

    for (auto& t : threads) t.join();

    EXPECT_EQ(popCount.load(), totalElements);
    EXPECT_TRUE(stack.isEmpty());
}

/**
 * @test Comprehensive stress test for simultaneous producers and consumers.
 * Verifies data integrity by comparing the sum of pushed values
 * against the sum of popped values (checksum validation).
 */
TEST(ModernThreadSafeLinkedStackTest, ProducerConsumerDataIntegrity) {
    ModernThreadSafeLinkedStack<int> stack;
    const int numProducers = 4;
    const int numConsumers = 4;
    const int itemsPerProducer = 5000;

    std::atomic<long long> sumPushed{ 0 };
    std::atomic<long long> sumPopped{ 0 };
    std::atomic<int> totalPoppedCount{ 0 };
    std::atomic<bool> producersDone{ false };

    // Producers push values and add to the total sum
    auto producerFunc = [&]() {
        for (int i = 1; i <= itemsPerProducer; ++i) {
            stack.push(i);
            sumPushed += i;
        }
        };

    // Consumers pop values as long as producers are active or stack is not empty
    auto consumerFunc = [&]() {
        while (!producersDone.load() || !stack.isEmpty()) {
            int val;
            if (stack.tryPop(val)) {
                sumPopped += val;
                totalPoppedCount++;
            }
            else {
                std::this_thread::yield(); // Minimal backoff
            }
        }
        };

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    for (int i = 0; i < numProducers; ++i) producers.emplace_back(producerFunc);
    for (int i = 0; i < numConsumers; ++i) consumers.emplace_back(consumerFunc);

    for (auto& p : producers) p.join();
    producersDone = true; // Signal consumers to finish once stack is empty
    for (auto& c : consumers) c.join();

    EXPECT_EQ(totalPoppedCount.load(), numProducers * itemsPerProducer);
    EXPECT_EQ(sumPushed.load(), sumPopped.load());
}