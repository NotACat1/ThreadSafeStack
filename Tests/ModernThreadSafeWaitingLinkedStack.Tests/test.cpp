#include "pch.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <memory>
#include <atomic>

#include "../../ThreadSafeStack/ModernThreadSafeWaitingLinkedStack.h"

using namespace std::chrono_literals;

/**
 * @category Functional Tests
 * @brief Basic validation of the linked stack's LIFO logic and state management.
 */

 // @test Verifies that a newly created linked stack is empty.
TEST(ModernThreadSafeWaitingLinkedStackTest, ConstructorInitializesEmpty) {
    ModernThreadSafeWaitingLinkedStack<int> stack;
    EXPECT_TRUE(stack.isEmpty());
}

// @test Validates push and tryPop operations, ensuring correct LIFO order.
TEST(ModernThreadSafeWaitingLinkedStackTest, PushAndTryPopIntegrity) {
    ModernThreadSafeWaitingLinkedStack<int> stack;
    stack.push(100);
    stack.push(200);

    int outValue = 0;
    EXPECT_TRUE(stack.tryPop(outValue));
    EXPECT_EQ(outValue, 200);

    auto ptr = stack.tryPop();
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, 100);
    EXPECT_TRUE(stack.isEmpty());
}

// @test Confirms that clear() resets the stack and handles iterative deletion.
TEST(ModernThreadSafeWaitingLinkedStackTest, ClearResetsStackState) {
    ModernThreadSafeWaitingLinkedStack<int> stack;
    for (int i = 0; i < 50; ++i) stack.push(i);

    stack.clear();
    EXPECT_TRUE(stack.isEmpty());
    EXPECT_EQ(stack.tryPop(), nullptr);
}

/**
 * @category Concurrency & Synchronization
 * @brief Tests blocking behavior, notification, and timeout logic.
 */

 // @test Validates that waitAndPop blocks a thread until data is pushed.
TEST(ModernThreadSafeWaitingLinkedStackTest, WaitAndPopBlocksCorrectly) {
    ModernThreadSafeWaitingLinkedStack<int> stack;
    int resultValue = 0;
    std::atomic<bool> finished{ false };

    std::thread consumer([&]() {
        stack.waitAndPop(resultValue);
        finished = true;
        });

    // Ensure consumer is waiting
    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(finished);

    stack.push(999);
    consumer.join();

    EXPECT_TRUE(finished);
    EXPECT_EQ(resultValue, 999);
}

// @test Verifies that waitAndPopFor accurately respects the specified timeout.
TEST(ModernThreadSafeWaitingLinkedStackTest, WaitAndPopForRespectsTimeout) {
    ModernThreadSafeWaitingLinkedStack<int> stack;
    int value = 0;

    auto start = std::chrono::steady_clock::now();
    bool popped = stack.waitAndPopFor(value, 100ms);
    auto end = std::chrono::steady_clock::now();

    EXPECT_FALSE(popped);
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_GE(duration.count(), 100);
}

// @test Verifies that waitAndPopFor succeeds if data arrives before timeout.
TEST(ModernThreadSafeWaitingLinkedStackTest, WaitAndPopForSucceedsWithinTimeout) {
    ModernThreadSafeWaitingLinkedStack<int> stack;
    int value = 0;

    std::thread producer([&]() {
        std::this_thread::sleep_for(30ms);
        stack.push(42);
        });

    bool popped = stack.waitAndPopFor(value, 200ms);
    producer.join();

    EXPECT_TRUE(popped);
    EXPECT_EQ(value, 42);
}

/**
 * @category Resource Management
 * @brief Checks for safety regarding deep stacks and move-only types.
 */

 // @test Ensures the iterative destructor prevents stack overflow on very deep stacks.
TEST(ModernThreadSafeWaitingLinkedStackTest, DeepStackDestructionIsSafe) {
    {
        ModernThreadSafeWaitingLinkedStack<int> stack;
        // 100k nodes would cause stack overflow with recursive unique_ptr destruction
        for (int i = 0; i < 100000; ++i) {
            stack.push(i);
        }
        // Destructor called here
    }
    SUCCEED();
}

// @test Validates that the stack handles move-only elements (e.g., unique_ptr wrappers).
TEST(ModernThreadSafeWaitingLinkedStackTest, HandlesMoveOnlyData) {
    ModernThreadSafeWaitingLinkedStack<std::unique_ptr<int>> stack;

    stack.push(std::make_unique<int>(777));

    auto resultPtr = stack.waitAndPop();
    ASSERT_NE(resultPtr, nullptr);
    ASSERT_NE(*resultPtr, nullptr);
    EXPECT_EQ(**resultPtr, 777);
}

/**
 * @category Stress Test
 * @brief Verifies integrity and thread-safety under heavy simultaneous load.
 */
TEST(ModernThreadSafeWaitingLinkedStackTest, HighContentionStressTest) {
    ModernThreadSafeWaitingLinkedStack<int> stack;
    const int numProducers = 6;
    const int numConsumers = 6;
    const int itemsPerProducer = 1500;

    std::atomic<int> totalCount{ 0 };
    std::atomic<long long> totalSum{ 0 };
    std::vector<std::thread> workers;

    // Producers
    for (int i = 0; i < numProducers; ++i) {
        workers.emplace_back([&stack, itemsPerProducer]() {
            for (int j = 1; j <= itemsPerProducer; ++j) {
                stack.push(j);
            }
            });
    }

    // Consumers
    for (int i = 0; i < numConsumers; ++i) {
        workers.emplace_back([&stack, itemsPerProducer, &totalCount, &totalSum]() {
            for (int j = 0; j < itemsPerProducer; ++j) {
                auto val = stack.waitAndPop();
                totalSum += *val;
                totalCount++;
            }
            });
    }

    for (auto& t : workers) t.join();

    long long expectedSumPerProducer = (1LL * itemsPerProducer * (itemsPerProducer + 1)) / 2;
    EXPECT_EQ(totalCount.load(), numProducers * itemsPerProducer);
    EXPECT_EQ(totalSum.load(), expectedSumPerProducer * numProducers);
    EXPECT_TRUE(stack.isEmpty());
}