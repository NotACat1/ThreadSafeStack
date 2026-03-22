#pragma once

#include "IStack.hpp"
#include <stack>
#include <mutex>
#include <memory>

/**
 * @brief A thread-safe stack implementation based on std::mutex.
 * * This class implements the IStack interface using a synchronized wrapper
 * around std::stack. It ensures thread safety for all operations by
 * utilizing a mutual exclusion (mutex) primitive to manage concurrent access.
 * * @tparam ElementType The type of elements stored in the stack.
 */
template <typename ElementType>
class MutexStack : public IStack<ElementType> {
private:
    /// The underlying non-thread-safe container.
    std::stack<ElementType> internalStack_;

    /// Mutex used to synchronize access to the internal stack.
    mutable std::mutex stackMutex_;

public:
    /**
     * @brief Default constructor.
     */
    MutexStack() = default;

    /**
     * @brief Copy assignment operator is disabled.
     * * Deleted to prevent unsafe copying of the mutex and to ensure
     * defined behavior in a concurrent environment.
     */
    MutexStack& operator=(const MutexStack&) = delete;

    /**
     * @brief Virtual destructor.
     */
    ~MutexStack() override = default;

    // --- IStack Interface Implementation ---

    /**
     * @brief Pushes a copy of the value onto the stack.
     * * @param value The element to be copied and inserted.
     */
    void push(const ElementType& value) override {
        std::lock_guard<std::mutex> lock(stackMutex_);
        internalStack_.push(value);
    }

    /**
     * @brief Pushes a value onto the stack using move semantics.
     * * @param value The element to be moved into the stack.
     */
    void push(ElementType&& value) override {
        std::lock_guard<std::mutex> lock(stackMutex_);
        internalStack_.push(std::move(value));
    }

    /**
     * @brief Removes and returns the top element.
     * * This method combines the top and pop operations into a single atomic step.
     * * @return A shared pointer to the removed element, or nullptr if the stack is empty.
     */
    std::shared_ptr<ElementType> pop() override {
        std::lock_guard<std::mutex> lock(stackMutex_);
        if (internalStack_.empty()) {
            return nullptr;
        }

        // Note: Used ElementType here to match the template parameter
        auto res = std::make_shared<ElementType>(std::move(internalStack_.top()));
        internalStack_.pop();
        return res;
    }

    /**
     * @brief Attempts to pop the top element into the provided reference.
     * * @param value Reference where the popped value will be moved if successful.
     * @return true if an element was popped; false if the stack was empty.
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
     * @brief Attempts to pop the top element and return it as a shared pointer.
     * * @return Shared pointer to the element, or nullptr if the stack is empty.
     */
    std::shared_ptr<ElementType> tryPop() override {
        // Delegates to pop() as the logic is identical for this implementation.
        return pop();
    }

    /**
     * @brief Thread-safe check for stack emptiness.
     * * @return true if the stack contains no elements, false otherwise.
     */
    bool isEmpty() const override {
        std::lock_guard<std::mutex> lock(stackMutex_);
        return internalStack_.empty();
    }

    /**
     * @brief Returns the current number of elements in the stack.
     * * @return Current stack size.
     */
    size_t getSize() const override {
        std::lock_guard<std::mutex> lock(stackMutex_);
        return internalStack_.size();
    }

    /**
     * @brief Removes all elements from the stack.
     * * This operation clears the entire stack contents under mutex protection.
     */
    void clear() override {
        std::lock_guard<std::mutex> lock(stackMutex_);
        while (!internalStack_.empty()) {
            internalStack_.pop();
        }
    }
};