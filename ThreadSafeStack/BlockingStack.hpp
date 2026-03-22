#pragma once

#include "IWaitingStack.hpp"
#include <stack>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <chrono>

/**
 * @brief A thread-safe LIFO stack implementation of the IWaitingStack interface.
 * * This class provides a synchronized wrapper around std::stack, supporting:
 * - Thread-safe push and pop operations.
 * - Blocking pops that wait for data availability.
 * - Timed-out waiting operations.
 * - Non-blocking status queries.
 * * @tparam ElementType The type of elements stored in the stack.
 */
template<typename ElementType>
class BlockingStack : public IWaitingStack<ElementType> {
private:
    /// Internal container for stack elements.
    std::stack<ElementType> internalStack_;

    /// Mutex protecting access to the internal container and size.
    mutable std::mutex stackMutex_;

    /// Condition variable to signal waiting threads when new data arrives.
    std::condition_variable dataCondition_;

public:
    /**
     * @brief Default constructor.
     */
    BlockingStack() = default;

    /**
     * @brief Copy constructor.
     * Performs a thread-safe copy of the source stack.
     * @param other The stack to copy from.
     */
    BlockingStack(const BlockingStack& other) {
        std::lock_guard<std::mutex> lock(other.stackMutex_);
        internalStack_ = other.internalStack_;
    }

    /**
     * @brief Assignment operator is deleted to prevent unsafe concurrent copying.
     */
    BlockingStack& operator=(const BlockingStack&) = delete;

    /**
     * @brief Move constructor.
     * Safely transfers ownership of the underlying stack.
     * @param other The stack to move from.
     */
    BlockingStack(BlockingStack&& other) noexcept {
        std::lock_guard<std::mutex> lock(other.stackMutex_);
        internalStack_ = std::move(other.internalStack_);
    }

    /**
     * @brief Virtual destructor.
     */
    ~BlockingStack() override = default;

    // --- IStack Implementation ---

    /**
     * @brief Pushes a copy of the value onto the stack.
     * Signals one waiting thread upon insertion.
     * @param value The value to be pushed.
     */
    void push(const ElementType& value) override {
        {
            std::lock_guard<std::mutex> lock(stackMutex_);
            internalStack_.push(value);
        }
        dataCondition_.notify_one();
    }

    /**
     * @brief Pushes a value onto the stack using move semantics.
     * Signals one waiting thread upon insertion.
     * @param value The value to be moved and pushed.
     */
    void push(ElementType&& value) override {
        {
            std::lock_guard<std::mutex> lock(stackMutex_);
            internalStack_.push(std::move(value));
        }
        dataCondition_.notify_one();
    }

    /**
     * @brief Removes and returns the top element.
     * @return Shared pointer to the popped element.
     * @throws std::out_of_range If the stack is empty.
     */
    std::shared_ptr<ElementType> pop() override {
        std::lock_guard<std::mutex> lock(stackMutex_);
        if (internalStack_.empty()) {
            throw std::out_of_range("BlockingStack::pop(): stack is empty");
        }
        auto res = std::make_shared<ElementType>(std::move(internalStack_.top()));
        internalStack_.pop();
        return res;
    }

    /**
     * @brief Attempts to pop the top element without blocking.
     * @param value Reference where the popped value will be stored.
     * @return true if an element was popped, false if the stack was empty.
     */
    bool tryPop(ElementType& value) override {
        std::lock_guard<std::mutex> lock(stackMutex_);
        if (internalStack_.empty()) {
            return false;
        }
        value = std::move(internalStack_.top());
        internalStack_.pop();
        return true;
    }

    /**
     * @brief Attempts to pop the top element without blocking.
     * @return Shared pointer to the element, or nullptr if the stack is empty.
     */
    std::shared_ptr<ElementType> tryPop() override {
        std::lock_guard<std::mutex> lock(stackMutex_);
        if (internalStack_.empty()) {
            return nullptr;
        }
        auto res = std::make_shared<ElementType>(std::move(internalStack_.top()));
        internalStack_.pop();
        return res;
    }

    /**
     * @brief Checks if the stack is empty.
     * @return true if empty, false otherwise.
     */
    bool isEmpty() const override {
        std::lock_guard<std::mutex> lock(stackMutex_);
        return internalStack_.empty();
    }

    /**
     * @brief Returns the current number of elements in the stack.
     * @return Current stack size.
     */
    size_t getSize() const override {
        std::lock_guard<std::mutex> lock(stackMutex_);
        return internalStack_.size();
    }

    /**
     * @brief Clears all elements from the stack.
     */
    void clear() override {
        std::lock_guard<std::mutex> lock(stackMutex_);
        while (!internalStack_.empty()) {
            internalStack_.pop();
        }
    }

    // --- IWaitingStack Implementation ---

    /**
     * @brief Blocks until an element is available, then removes and stores it.
     * @param value Reference to store the popped value.
     */
    void waitAndPop(ElementType& value) override {
        std::unique_lock<std::mutex> lock(stackMutex_);
        dataCondition_.wait(lock, [this] { return !internalStack_.empty(); });

        value = std::move(internalStack_.top());
        internalStack_.pop();
    }

    /**
     * @brief Blocks until an element is available, then removes and returns it.
     * @return Shared pointer to the popped element.
     */
    std::shared_ptr<ElementType> waitAndPop() override {
        std::unique_lock<std::mutex> lock(stackMutex_);
        dataCondition_.wait(lock, [this] { return !internalStack_.empty(); });

        auto res = std::make_shared<ElementType>(std::move(internalStack_.top()));
        internalStack_.pop();
        return res;
    }

    /**
     * @brief Waits for an element with a timeout.
     * @param value Reference to store the popped value.
     * @param timeout Maximum duration to wait.
     * @return true if an element was popped before the timeout expired, false otherwise.
     */
    bool waitAndPopFor(ElementType& value, const std::chrono::milliseconds& timeout) override {
        std::unique_lock<std::mutex> lock(stackMutex_);
        if (!dataCondition_.wait_for(lock, timeout, [this] { return !internalStack_.empty(); })) {
            return false;
        }

        value = std::move(internalStack_.top());
        internalStack_.pop();
        return true;
    }

    /**
     * @brief Pushes multiple elements onto the stack.
     * Notifies all waiting threads after insertion.
     * @param elements Initializer list of elements to insert.
     */
    void push_multiple(std::initializer_list<ElementType> elements) {
        {
            std::lock_guard<std::mutex> lock(stackMutex_);
            for (auto& elem : elements) {
                // move-casts are necessary as elements in initializer_list are const
                internalStack_.push(std::move(const_cast<ElementType&>(elem)));
            }
        }
        dataCondition_.notify_all();
    }
};