#include "pch.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <memory>

#include "../../ThreadSafeStack/SharedPtrWaitingThreadSafeStack.h"

using namespace std::chrono_literals;

/**
 * @category Functional Tests
 * @brief Basic validation of the stack's LIFO logic and state management.
 */

 // @test Validates that the stack is correctly initialized without any elements.
TEST(SharedPtrWaitingThreadSafeStackTest, ConstructorInitializesEmpty) {
    SharedPtrWaitingThreadSafeStack<int> stack;
    EXPECT_TRUE(stack.isEmpty());
    EXPECT_EQ(stack.getSize(), 0);
}

// @test Verifies that pushing elements increases size and preserves LIFO order.
TEST(SharedPtrWaitingThreadSafeStackTest, PushAndTryPopValueIntegrity) {
    SharedPtrWaitingThreadSafeStack<std::string> stack;
    stack.push("Alpha");
    stack.push("Beta");

    EXPECT_EQ(stack.getSize(), 2);

    std::string out;
    EXPECT_TRUE(stack.tryPop(out));
    EXPECT_EQ(out, "Beta");

    auto ptr = stack.tryPop();
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, "Alpha");
}

// @test Ensures that clear() effectively removes all elements and resets size.
TEST(SharedPtrWaitingThreadSafeStackTest, ClearResetsStackState) {
    SharedPtrWaitingThreadSafeStack<int> stack;
    for (int i = 0; i < 100; ++i) stack.push(i);

    EXPECT_EQ(stack.getSize(), 100);
    stack.clear();
    EXPECT_TRUE(stack.isEmpty());
    EXPECT_EQ(stack.getSize(), 0);
}

/**
 * @category Concurrency & Synchronization
 * @brief Tests the blocking behavior and thread-safety of the implementation.
 */

 // @test Validates the producer-consumer pattern using blocking waitAndPop.
TEST(SharedPtrWaitingThreadSafeStackTest, WaitAndPopBlocksUntilDataIsPushed) {
    SharedPtrWaitingThreadSafeStack<int> stack;
    int poppedValue = 0;
    std::atomic<bool> threadFinished{ false };

    std::thread consumer([&]() {
        stack.waitAndPop(poppedValue);
        threadFinished = true;
        });

    // Brief delay to ensure consumer is in waiting state
    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(threadFinished);

    stack.push(123);
    consumer.join();

    EXPECT_TRUE(threadFinished);
    EXPECT_EQ(poppedValue, 123);
}

/**
 * @test Verifies that waitAndPopFor accurately respects the timeout
 * when the stack remains empty.
 */
TEST(SharedPtrWaitingThreadSafeStackTest, WaitAndPopForRespectsTimeout) {
    SharedPtrWaitingThreadSafeStack<int> stack;
    int value = 0;

    auto start = std::chrono::steady_clock::now();
    bool result = stack.waitAndPopFor(value, 150ms);
    auto end = std::chrono::steady_clock::now();

    EXPECT_FALSE(result);
    EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(), 140);
}

/**
 * @category Memory & Resource Management
 * @brief Checks for proper handling of shared_ptr and move semantics.
 */

 // @test Ensures that the stack correctly moves non-copyable or large objects.
TEST(SharedPtrWaitingThreadSafeStackTest, HandlesMoveOnlyObjects) {
    // Using unique_ptr inside shared_ptr logic is a good test for move-heavy paths
    SharedPtrWaitingThreadSafeStack<std::unique_ptr<int>> stack;

    auto item = std::make_unique<int>(1000);
    stack.push(std::move(item));

    auto poppedPtr = stack.waitAndPop();
    ASSERT_NE(poppedPtr, nullptr);
    ASSERT_NE(*poppedPtr, nullptr);
    EXPECT_EQ(**poppedPtr, 1000);
}

// @test Verifies that the copy constructor performs a thread-safe deep copy.
TEST(SharedPtrWaitingThreadSafeStackTest, CopyConstructorThreadSafety) {
    SharedPtrWaitingThreadSafeStack<int> original;
    original.push(1);
    original.push(2);

    SharedPtrWaitingThreadSafeStack<int> copy(original);

    EXPECT_EQ(copy.getSize(), 2);
    EXPECT_EQ(*copy.tryPop(), 2);
    // Original should remain intact
    EXPECT_EQ(original.getSize(), 2);
}

/**
 * @category Stress Test
 * @brief High-contention test to ensure data integrity under heavy load.
 */
TEST(SharedPtrWaitingThreadSafeStackTest, HighContentionStressTest) {
    SharedPtrWaitingThreadSafeStack<long long> stack;
    const int producersCount = 10;
    const int consumersCount = 10;
    const int itemsPerProducer = 1000;

    std::atomic<long long> totalSumPopped{ 0 };
    std::vector<std::thread> threads;

    // Launch producers
    for (int i = 0; i < producersCount; ++i) {
        threads.emplace_back([&stack, itemsPerProducer]() {
            for (int j = 1; j <= itemsPerProducer; ++j) {
                stack.push(j);
            }
            });
    }

    // Launch consumers
    for (int i = 0; i < consumersCount; ++i) {
        threads.emplace_back([&stack, &totalSumPopped, itemsPerProducer]() {
            for (int j = 0; j < itemsPerProducer; ++j) {
                totalSumPopped += *stack.waitAndPop();
            }
            });
    }

    for (auto& t : threads) t.join();

    // Verification via arithmetic progression sum: n * (n + 1) / 2
    long long expectedSumPerProducer = (1LL * itemsPerProducer * (itemsPerProducer + 1)) / 2;
    long long totalExpectedSum = expectedSumPerProducer * producersCount;

    EXPECT_EQ(totalSumPopped.load(), totalExpectedSum);
    EXPECT_TRUE(stack.isEmpty());
}