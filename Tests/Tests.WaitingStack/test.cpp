#include "pch.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "../../ThreadSafeStack/BlockingStack.hpp"
#include "../../ThreadSafeStack/BlockingLinkedStack.hpp"

/**
 * @brief Type-parameterized test fixture for stacks supporting blocking operations.
 * Allows running the same test logic against different implementations.
 */
template <typename WaitingStackImplementation>
class IWaitingStackTest : public ::testing::Test {
protected:
    WaitingStackImplementation stack;
};

// List of implementations that satisfy the IWaitingStack interface (supporting waitAndPop)
using IWaitingStackImplementations = ::testing::Types<
    BlockingStack<int>,
    BlockingLinkedStack<int>
>;
TYPED_TEST_CASE(IWaitingStackTest, IWaitingStackImplementations);

// ---------------------------------------------------------
// Thread-safety tests for IWaitingStack blocking semantics
// ---------------------------------------------------------

/**
 * @test Verifies that waitAndPop effectively blocks the calling thread
 * until data is pushed into the stack.
 */
TYPED_TEST(IWaitingStackTest, WaitAndPopBlocksUntilDataAvailable) {
    std::atomic<bool> popped{ false };
    int val = 0;

    // Launch a consumer thread that is expected to block
    std::thread consumer([this, &val, &popped]() {
        this->stack.waitAndPop(val);
        popped = true;
        });

    // Induce a short delay to ensure the consumer thread has time to enter the waiting state
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(popped.load()); // Thread should still be blocked

    // Push data to unblock the consumer
    this->stack.push(99);
    consumer.join();

    EXPECT_TRUE(popped.load());
    EXPECT_EQ(val, 99);
}

/**
 * @test Ensures the shared_ptr overload of waitAndPop correctly blocks and retrieves data.
 */
TYPED_TEST(IWaitingStackTest, WaitAndPopReturnsSharedPtrCorrectly) {
    std::atomic<bool> completed{ false };
    std::shared_ptr<int> result;

    std::thread consumer([this, &result, &completed]() {
        result = this->stack.waitAndPop();
        completed = true;
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(completed.load());

    this->stack.push(42);
    consumer.join();

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, 42);
}

/**
 * @test Validates the timeout logic of waitAndPopFor when no data is pushed.
 */
TYPED_TEST(IWaitingStackTest, WaitAndPopForTimesOutCorrectly) {
    int val = 0;
    auto start = std::chrono::steady_clock::now();

    // Set a 50ms timeout
    bool success = this->stack.waitAndPopFor(val, std::chrono::milliseconds(50));
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_FALSE(success);
    // Verify that the actual delay was no less than the requested timeout 
    // (allowing for ~5ms OS scheduling jitter)
    EXPECT_GE(duration.count(), 45);
}

/**
 * @test Verifies that waitAndPopFor returns immediately if data arrives before the timeout.
 */
TYPED_TEST(IWaitingStackTest, WaitAndPopForSucceedsIfDataArrives) {
    int val = 0;

    // Producer thread: pushes data after a 20ms delay
    std::thread producer([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        this->stack.push(777);
        });

    // Consumer thread: waits for up to 100ms
    bool success = this->stack.waitAndPopFor(val, std::chrono::milliseconds(100));
    producer.join();

    EXPECT_TRUE(success);
    EXPECT_EQ(val, 777);
}
