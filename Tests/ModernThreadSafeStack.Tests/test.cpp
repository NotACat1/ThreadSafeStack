#include "pch.h"

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <string>
#include "../../ThreadSafeStack/ModernThreadSafeStack.h"

/**
 * @category Functional Tests (Single-threaded)
 * @brief These tests verify the core logic of the stack in a deterministic,
 * single-threaded environment to ensure basic LIFO behavior.
 */

 // @test Verifies that a newly created stack is empty and has a size of zero.
TEST(ModernThreadSafeStackTest, InitialStateIsCorrect) {
    ModernThreadSafeStack<int> stack;
    EXPECT_TRUE(stack.isEmpty());
    EXPECT_EQ(stack.getSize(), 0);
}

// @test Validates basic push and pop operations, ensuring LIFO order and proper size updates.
TEST(ModernThreadSafeStackTest, PushAndPopValues) {
    ModernThreadSafeStack<int> stack;
    stack.push(10);
    stack.push(20);

    EXPECT_FALSE(stack.isEmpty());
    EXPECT_EQ(stack.getSize(), 2);

    int val = 0;
    stack.pop(val); // Pop 20
    EXPECT_EQ(val, 20);

    auto ptr = stack.pop(); // Pop 10
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, 10);
    EXPECT_TRUE(stack.isEmpty());
}

// @test Ensures that popping from an empty stack throws a std::out_of_range exception.
TEST(ModernThreadSafeStackTest, PopEmptyStackThrowsException) {
    ModernThreadSafeStack<int> stack;
    int val = 0;

    EXPECT_THROW(stack.pop(), std::out_of_range);
    EXPECT_THROW(stack.pop(val), std::out_of_range);
}

// @test Tests non-throwing pop variants (tryPop) for both empty and non-empty stack scenarios.
TEST(ModernThreadSafeStackTest, TryPopWorksCorrectly) {
    ModernThreadSafeStack<int> stack;
    int val = 0;

    // Test tryPop on empty stack
    EXPECT_FALSE(stack.tryPop(val));
    EXPECT_EQ(stack.tryPop(), nullptr);

    stack.push(42);

    // Test successful tryPop
    EXPECT_TRUE(stack.tryPop(val));
    EXPECT_EQ(val, 42);
    EXPECT_TRUE(stack.isEmpty());
}

// @test Confirms that the clear() method removes all elements and resets the stack state.
TEST(ModernThreadSafeStackTest, ClearEmptiesTheStack) {
    ModernThreadSafeStack<std::string> stack;
    stack.push("data");
    stack.clear();

    EXPECT_TRUE(stack.isEmpty());
    EXPECT_EQ(stack.getSize(), 0);
}

// @test Verifies thread-safe deep copying and ensures the source stack remains intact.
TEST(ModernThreadSafeStackTest, CopyConstructorWorks) {
    ModernThreadSafeStack<int> original;
    original.push(100);

    ModernThreadSafeStack<int> copy(original);

    EXPECT_EQ(copy.getSize(), 1);
    int val = 0;
    copy.pop(val);
    EXPECT_EQ(val, 100);
    EXPECT_EQ(original.getSize(), 1); // Source must be unchanged
}

// @test Verifies move semantics, ensuring resources are transferred and the source is left valid.
TEST(ModernThreadSafeStackTest, MoveConstructorWorks) {
    ModernThreadSafeStack<int> original;
    original.push(500);

    ModernThreadSafeStack<int> moved(std::move(original));

    EXPECT_EQ(moved.getSize(), 1);
    EXPECT_TRUE(original.isEmpty()); // Source should be empty after move
}

/**
 * @category Concurrency Tests (Multi-threaded)
 * @brief These tests stress the implementation under high contention to detect
 * race conditions, deadlocks, or data corruption.
 */

 // @test Verifies thread safety during simultaneous push operations from multiple threads.
TEST(ModernThreadSafeStackTest, ConcurrentPushes) {
    ModernThreadSafeStack<int> stack;
    const int numThreads = 10;
    const int numPushesPerThread = 1000;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&stack, numPushesPerThread]() {
            for (int j = 0; j < numPushesPerThread; ++j) {
                stack.push(j);
            }
            });
    }

    for (auto& t : threads) t.join();

    EXPECT_EQ(stack.getSize(), numThreads * numPushesPerThread);
}

// @test Ensures no data loss or double-popping occurs when multiple threads consume the stack.
TEST(ModernThreadSafeStackTest, ConcurrentPops) {
    ModernThreadSafeStack<int> stack;
    const int totalElements = 5000;
    for (int i = 0; i < totalElements; ++i) stack.push(i);

    std::atomic<int> popCount{ 0 };
    const int numThreads = 8;
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
 * Verifies overall data integrity by comparing the sum of pushed values
 * against the sum of popped values (checksum validation).
 */
TEST(ModernThreadSafeStackTest, ConcurrentPushAndPopDataIntegrity) {
    ModernThreadSafeStack<int> stack;
    const int numThreads = 4;
    const int itemsPerThread = 2000;

    std::atomic<int> totalPopped{ 0 };
    std::atomic<bool> producersDone{ false };
    std::atomic<long long> sumPushed{ 0 };
    std::atomic<long long> sumPopped{ 0 };

    auto producer = [&]() {
        for (int j = 1; j <= itemsPerThread; ++j) {
            stack.push(j);
            sumPushed += j;
        }
        };

    auto consumer = [&]() {
        int val;
        while (!producersDone || !stack.isEmpty()) {
            if (stack.tryPop(val)) {
                totalPopped++;
                sumPopped += val;
            }
            else {
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

    EXPECT_EQ(totalPopped.load(), numThreads * itemsPerThread);
    EXPECT_EQ(sumPushed.load(), sumPopped.load());
}