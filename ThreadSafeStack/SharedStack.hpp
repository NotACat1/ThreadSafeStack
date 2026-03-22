#pragma once

#include "IWaitingStack.hpp"
#include <stack>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <chrono>
#include <stdexcept>

/**
 * @brief A thread-safe LIFO stack implementation using std::shared_ptr for internal storage.
 * * This class implements the IWaitingStack interface. By storing elements as std::shared_ptr,
 * the stack minimizes expensive copy operations and provides strong exception safety
 * guarantees. It is ideal for handling large objects or non-trivial types in
 * highly concurrent environments.
 * * Key Features:
 * - Thread-safe synchronization via std::mutex.
 * - Efficient resource management using shared ownership.
 * - Support for blocking and timed waiting operations via std::condition_variable.
 * * @tparam ElementType The type of elements managed by the stack.
 */
template<typename ElementType>
class SharedStack : public IWaitingStack<ElementType> {
private:
    /// Underlying container storing shared pointers to elements.
    std::stack<std::shared_ptr<ElementType>> internalStack_;

    /// Mutex used to synchronize access to the internal stack.
    mutable std::mutex stackMutex_;

    /// Condition variable to notify waiting threads when new data arrives.
    std::condition_variable dataCondition_;

public:
    /**
     * @brief Default constructor.
     */
    SharedStack() = default;

    /**
     * @brief Thread-safe copy constructor.
     * Locks the source stack to ensure a consistent state during the copy operation.
     * @param other The source stack instance to copy from.
     */
    SharedStack(const SharedStack& other) {
        std::lock_guard<std::mutex> lock(other.stackMutex_);
        internalStack_ = other.internalStack_;
    }

    /**
     * @brief Copy assignment operator is disabled to prevent unsafe concurrent assignment.
     */
    SharedStack& operator=(const SharedStack&) = delete;

    /**
     * @brief Thread-safe move constructor.
     * @param other The source stack instance to move from.
     */
    SharedStack(SharedStack&& other) noexcept {
        std::lock_guard<std::mutex> lock(other.stackMutex_);
        internalStack_ = std::move(other.internalStack_);
    }

    /**
     * @brief Virtual destructor.
     */
    ~SharedStack() override = default;

    // --- IStack Implementation ---

    /**
     * @brief Pushes a copy of the value onto the stack.
     * Wraps the value in a shared_ptr before acquiring the lock to minimize the critical section.
     * @param value The value to be copied and pushed.
     */
    void push(const ElementType& value) override {
        auto data = std::make_shared<ElementType>(value);
        {
            std::lock_guard<std::mutex> lock(stackMutex_);
            internalStack_.push(data);
        }
        dataCondition_.notify_one();
    }

    /**
     * @brief Pushes a value onto the stack using move semantics.
     * Memory allocation for the shared_ptr occurs outside the lock to reduce contention.
     * @param value The rvalue to be moved and pushed.
     */
    void push(ElementType&& value) override {
        auto data = std::make_shared<ElementType>(std::move(value));
        {
            std::lock_guard<std::mutex> lock(stackMutex_);
            internalStack_.push(data);
        }
        dataCondition_.notify_one();
    }

    /**
     * @brief Removes the top element and returns it.
     * @return Shared pointer to the popped element.
     * @throws std::runtime_error If the stack is empty.
     */
    std::shared_ptr<ElementType> pop() override {
        std::lock_guard<std::mutex> lock(stackMutex_);
        if (internalStack_.empty()) {
            throw std::runtime_error("SharedStack::pop(): stack is empty");
        }
        auto result = internalStack_.top();
        internalStack_.pop();
        return result;
    }

    /**
     * @brief Attempts to pop the top element without blocking.
     * @param value Reference to store the popped value if successful.
     * @return true if an element was popped, false if the stack was empty.
     */
    bool tryPop(ElementType& value) override {
        std::lock_guard<std::mutex> lock(stackMutex_);
        if (internalStack_.empty()) return false;

        value = std::move(*internalStack_.top());
        internalStack_.pop();
        return true;
    }

    /**
     * @brief Attempts to pop the top element and return a shared pointer.
     * @return Shared pointer to the element, or nullptr if empty.
     */
    std::shared_ptr<ElementType> tryPop() override {
        std::lock_guard<std::mutex> lock(stackMutex_);
        if (internalStack_.empty()) return nullptr;

        auto result = internalStack_.top();
        internalStack_.pop();
        return result;
    }

    /**
     * @brief Checks if the stack is currently empty.
     * @return true if empty, false otherwise.
     */
    bool isEmpty() const override {
        std::lock_guard<std::mutex> lock(stackMutex_);
        return internalStack_.empty();
    }

    /**
     * @brief Returns the number of elements in the stack.
     * @return Current stack size.
     */
    size_t getSize() const override {
        std::lock_guard<std::mutex> lock(stackMutex_);
        return internalStack_.size();
    }

    /**
     * @brief Removes all elements from the stack.
     */
    void clear() override {
        std::lock_guard<std::mutex> lock(stackMutex_);
        while (!internalStack_.empty()) {
            internalStack_.pop();
        }
    }

    // --- IWaitingStack Implementation ---

    /**
     * @brief Blocks until an element is available and pops it.
     * @param value Reference to receive the popped value.
     */
    void waitAndPop(ElementType& value) override {
        std::unique_lock<std::mutex> lock(stackMutex_);
        dataCondition_.wait(lock, [this] { return !internalStack_.empty(); });

        value = std::move(*internalStack_.top());
        internalStack_.pop();
    }

    /**
     * @brief Blocks until an element is available and returns it as a shared_ptr.
     * @return Shared pointer to the popped element.
     */
    std::shared_ptr<ElementType> waitAndPop() override {
        std::unique_lock<std::mutex> lock(stackMutex_);
        dataCondition_.wait(lock, [this] { return !internalStack_.empty(); });

        auto result = internalStack_.top();
        internalStack_.pop();
        return result;
    }

    /**
     * @brief Blocks for a limited time until an element is available.
     * @param value Reference to receive the popped value.
     * @param timeout Maximum duration to wait.
     * @return true if an element was popped before timeout, false otherwise.
     */
    bool waitAndPopFor(ElementType& value, const std::chrono::milliseconds& timeout) override {
        std::unique_lock<std::mutex> lock(stackMutex_);
        if (!dataCondition_.wait_for(lock, timeout, [this] { return !internalStack_.empty(); })) {
            return false;
        }

        value = std::move(*internalStack_.top());
        internalStack_.pop();
        return true;
    }
};