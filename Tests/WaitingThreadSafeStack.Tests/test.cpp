#include "pch.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>

#include "../../ThreadSafeStack/WaitingThreadSafeStack.h"

using namespace std::chrono_literals;

/**
 * @category Unit Tests - Basic Functionality
 * @brief Tests the standard stack operations in a single-threaded context.
 */

 // @test Ensures the stack is correctly initialized as empty.
TEST(WaitingThreadSafeStackTest, InitialStateIsEmpty) {
    WaitingThreadSafeStack<int> stack;
    EXPECT_TRUE(stack.isEmpty());
    EXPECT_EQ(stack.getSize(), 0);
}

// @test Verifies basic push and pop operations with LIFO (Last-In-First-Out) order.
TEST(WaitingThreadSafeStackTest, PushAndPopVerifiesLIFO) {
    WaitingThreadSafeStack<int> stack;
    stack.push(10);
    stack.push(20);

    EXPECT_EQ(stack.getSize(), 2);

    int result = 0;
    stack.pop(result);
    EXPECT_EQ(result, 20);

    auto ptr = stack.pop();
    EXPECT_EQ(*ptr, 10);
    EXPECT_TRUE(stack.isEmpty());
}

// @test Checks that top() provides access to the element without removing it.
TEST(WaitingThreadSafeStackTest, TopReturnsElementWithoutPopping) {
    WaitingThreadSafeStack<std::string> stack;
    stack.push("first");

    EXPECT_EQ(stack.top(), "first");
    EXPECT_EQ(stack.getSize(), 1); // Size should remain 1
}

// @test Verifies that pushing multiple elements via initializer list works and notifies correctly.
TEST(WaitingThreadSafeStackTest, PushMultipleInitializesCorrectly) {
    WaitingThreadSafeStack<int> stack;
    stack.push_multiple({ 1, 2, 3, 4, 5 });

    EXPECT_EQ(stack.getSize(), 5);
    EXPECT_EQ(stack.top(), 5);
}

// @test Ensures proper exception handling when accessing an empty stack.
TEST(WaitingThreadSafeStackTest, ThrowsExceptionOnEmptyStack) {
    WaitingThreadSafeStack<int> stack;
    int value;

    EXPECT_THROW(stack.pop(), std::out_of_range);
    EXPECT_THROW(stack.pop(value), std::out_of_range);
    EXPECT_THROW(stack.top(), std::out_of_range);
}

/**
 * @category Concurrency Tests - Blocking and Timed Operations
 * @brief Tests how the stack handles threads waiting for data and time-based operations.
 */

 // @test Verifies that waitAndPop blocks until another thread pushes data.
TEST(WaitingThreadSafeStackTest, WaitAndPopBlocksUntilDataIsAvailable) {
    WaitingThreadSafeStack<int> stack;
    int poppedValue = 0;

    // Run waitAndPop in a separate thread
    std::thread consumer([&]() {
        stack.waitAndPop(poppedValue);
        });

    // Simulate some work
    std::this_thread::sleep_for(100ms);
    EXPECT_EQ(poppedValue, 0); // Still 0 because stack is empty

    stack.push(42); // This should wake up the consumer

    if (consumer.joinable()) {
        consumer.join();
    }

    EXPECT_EQ(poppedValue, 42);
}

// @test Verifies that waitAndPopFor returns false after the specified timeout.
TEST(WaitingThreadSafeStackTest, WaitAndPopForTimesOutOnEmptyStack) {
    WaitingThreadSafeStack<int> stack;
    int value = -1;

    auto start = std::chrono::steady_clock::now();
    bool success = stack.waitAndPopFor(value, 200ms);
    auto end = std::chrono::steady_clock::now();

    EXPECT_FALSE(success);
    EXPECT_GE(end - start, 200ms); // Ensure it actually waited
}

// @test Verifies that waitAndPopFor returns true if data is pushed before timeout.
TEST(WaitingThreadSafeStackTest, WaitAndPopForSucceedsWithinTimeout) {
    WaitingThreadSafeStack<int> stack;

    std::thread producer([&]() {
        std::this_thread::sleep_for(50ms);
        stack.push(99);
        });

    auto result = stack.waitAndPopFor(500ms);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, 99);

    if (producer.joinable()) producer.join();
}

/**
 * @category Thread-Safety Stress Tests
 * @brief Heavy contention tests to verify data integrity and absence of race conditions.
 */

 // @test Stress test with multiple producers and consumers.
TEST(WaitingThreadSafeStackTest, MultiThreadedProducersAndConsumers) {
    WaitingThreadSafeStack<int> stack;
    const int numProducers = 4;
    const int numConsumers = 4;
    const int itemsPerProducer = 1000;
    const int totalItems = numProducers * itemsPerProducer;

    std::atomic<int> consumeCount{ 0 };
    std::atomic<long long> checksum{ 0 };

    auto producerFunc = [&](int id) {
        for (int i = 0; i < itemsPerProducer; ++i) {
            stack.push(id * 10000 + i);
        }
        };

    auto consumerFunc = [&]() {
        for (int i = 0; i < itemsPerProducer; ++i) {
            int val;
            stack.waitAndPop(val);
            checksum += val;
            consumeCount++;
        }
        };

    std::vector<std::thread> threads;
    for (int i = 0; i < numProducers; ++i) threads.emplace_back(producerFunc, i);
    for (int i = 0; i < numConsumers; ++i) threads.emplace_back(consumerFunc);

    for (auto& t : threads) t.join();

    EXPECT_EQ(consumeCount.load(), totalItems);
    EXPECT_TRUE(stack.isEmpty());
}

// @test Verifies that copy constructor safely captures the state under contention.
TEST(WaitingThreadSafeStackTest, CopyConstructorIsThreadSafe) {
    WaitingThreadSafeStack<int> stack;

    // Fill the stack
    for (int i = 0; i < 100; ++i) stack.push(i);

    // Thread trying to push more elements while we copy
    std::thread pusher([&]() {
        for (int i = 100; i < 200; ++i) stack.push(i);
        });

    // Perform copy
    WaitingThreadSafeStack<int> copyStack(stack);

    pusher.join();

    // The copy should have at least the initial 100 elements
    EXPECT_GE(copyStack.getSize(), 100);
}

// @test Verifies that move constructor transfers ownership and leaves source valid.
TEST(WaitingThreadSafeStackTest, MoveConstructorTransfersData) {
    WaitingThreadSafeStack<int> source;
    source.push(777);

    size_t oldSize = source.getSize();
    WaitingThreadSafeStack<int> destination(std::move(source));

    EXPECT_EQ(destination.getSize(), oldSize);
    EXPECT_EQ(destination.top(), 777);
    // Note: source internal container state depends on std::stack move, 
    // but the object itself remains in a valid state.
}