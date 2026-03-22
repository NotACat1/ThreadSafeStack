
#include "pch.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include "../../../ThreadSafeStack/MutexStack.hpp"
#include "../../../ThreadSafeStack/BlockingStack .hpp"
#include "../../../ThreadSafeStack/SharedStack.hpp"
#include "../../../ThreadSafeStack/LinkedStack.hpp"
#include "../../../ThreadSafeStack/BlockingLinkedStack.hpp"

// ---------------------------------------------------------
// 1. Base IStack Interface Test Fixture
// ---------------------------------------------------------

/**
 * @brief Generic test fixture for standard stack operations.
 */
template <typename StackImplementation>
class IStackTest : public ::testing::Test {
protected:
    StackImplementation stack;
};

// Register all stack implementations for standard testing
using IStackImplementations = ::testing::Types<
    MutexStack<int>,
    BlockingStack<int>,
    SharedStack<int>,
    LinkedStack<int>,
    BlockingLinkedStack<int>
>;

TYPED_TEST_CASE(IStackTest, IStackImplementations);

// ---------------------------------------------------------
// Functional and Thread-Safety tests for IStack
// ---------------------------------------------------------

TYPED_TEST(IStackTest, InitialStateIsCorrect) {
    EXPECT_TRUE(this->stack.isEmpty());
    EXPECT_EQ(this->stack.getSize(), 0);
}

TYPED_TEST(IStackTest, PushAndPopValues) {
    this->stack.push(10);
    this->stack.push(20);

    EXPECT_FALSE(this->stack.isEmpty());
    EXPECT_EQ(this->stack.getSize(), 2);

    int val = 0;
    // Test reference-based tryPop
    EXPECT_TRUE(this->stack.tryPop(val));
    EXPECT_EQ(val, 20);

    // Test shared_ptr-based pop
    auto ptr = this->stack.pop();
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, 10);

    EXPECT_TRUE(this->stack.isEmpty());
}

TYPED_TEST(IStackTest, TryPopWorksCorrectly) {
    int val = 0;

    // Verify non-blocking pop behavior on an empty stack
    EXPECT_FALSE(this->stack.tryPop(val));
    EXPECT_EQ(this->stack.tryPop(), nullptr);

    this->stack.push(42);

    // Verify successful pop
    EXPECT_TRUE(this->stack.tryPop(val));
    EXPECT_EQ(val, 42);
    EXPECT_TRUE(this->stack.isEmpty());
}

TYPED_TEST(IStackTest, ClearEmptiesTheStack) {
    this->stack.push(100);
    this->stack.push(200);
    this->stack.clear();

    EXPECT_TRUE(this->stack.isEmpty());
    EXPECT_EQ(this->stack.getSize(), 0);
    EXPECT_EQ(this->stack.tryPop(), nullptr);
}

/** * @category Concurrency Tests
 * @brief Stress tests to ensure thread-safety and race-condition resistance.
 */

 /**
  * @test Verifies that the stack size is consistent after high-frequency concurrent pushes.
  */
TYPED_TEST(IStackTest, ConcurrentPushes) {
    const int numThreads = 10;
    const int numPushesPerThread = 1000;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, numPushesPerThread]() {
            for (int j = 0; j < numPushesPerThread; ++j) {
                this->stack.push(j);
            }
            });
    }

    for (auto& t : threads) t.join();

    EXPECT_EQ(this->stack.getSize(), numThreads * numPushesPerThread);
}

/**
 * @test Verifies that multiple threads can concurrently pop elements without double-freeing or missing data.
 */
TYPED_TEST(IStackTest, ConcurrentPops) {
    const int totalElements = 5000;
    for (int i = 0; i < totalElements; ++i) {
        this->stack.push(i);
    }

    std::atomic<int> popCount{ 0 };
    const int numThreads = 8;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, &popCount]() {
            int val;
            while (this->stack.tryPop(val)) {
                popCount++;
            }
            });
    }

    for (auto& t : threads) t.join();

    EXPECT_EQ(popCount.load(), totalElements);
    EXPECT_TRUE(this->stack.isEmpty());
}

/**
 * @test Validates data integrity (sum and count) under simultaneous push and pop operations.
 */
TYPED_TEST(IStackTest, ConcurrentPushAndPopDataIntegrity) {
    const int numThreads = 4;
    const int itemsPerThread = 2000;

    std::atomic<int> totalPopped{ 0 };
    std::atomic<bool> producersDone{ false };
    std::atomic<long long> sumPushed{ 0 };
    std::atomic<long long> sumPopped{ 0 };

    auto producer = [this, itemsPerThread, &sumPushed]() {
        for (int j = 1; j <= itemsPerThread; ++j) {
            this->stack.push(j);
            sumPushed += j;
        }
        };

    auto consumer = [this, &producersDone, &totalPopped, &sumPopped]() {
        int val;
        while (!producersDone.load() || !this->stack.isEmpty()) {
            if (this->stack.tryPop(val)) {
                totalPopped++;
                sumPopped += val;
            }
            else {
                // Yield to prevent tight-looping while waiting for producers
                std::this_thread::yield();
            }
        }
        };

    std::vector<std::thread> producers, consumers;
    for (int i = 0; i < numThreads; ++i) {
        producers.emplace_back(producer);
        consumers.emplace_back(consumer);
    }

    for (auto& t : producers) t.join();
    producersDone = true;
    for (auto& t : consumers) t.join();

    // Final checks: ensure no elements were lost and the total sum matches
    EXPECT_EQ(totalPopped.load(), numThreads * itemsPerThread);
    EXPECT_EQ(sumPushed.load(), sumPopped.load());
}