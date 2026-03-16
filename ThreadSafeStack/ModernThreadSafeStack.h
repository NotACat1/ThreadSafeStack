#pragma once

#include <stack>
#include <mutex>
#include <stdexcept>
#include <memory>

/**
 * @brief A thread-safe stack implementation using modern C++ concurrency primitives.
 *
 * This class provides a synchronized wrapper around std::stack to ensure
 * safe concurrent access from multiple threads. All operations that modify
 * or read the stack are protected by a mutex.
 *
 * The class supports both exception-based and non-throwing pop operations.
 *
 * @tparam ElementType Type of elements stored in the stack.
 */
template<typename ElementType>
class ModernThreadSafeStack
{
private:
    /// Underlying container storing stack elements
    std::stack<ElementType> internalStack_;

    /// Mutex protecting access to the stack
    mutable std::mutex stackMutex_;

public:
    /**
     * @brief Default constructor.
     */
    ModernThreadSafeStack() = default;

    /**
     * @brief Copy constructor.
     *
     * Creates a copy of another stack in a thread-safe manner.
     * Locks the source stack during copying.
     *
     * @param otherStack Stack to copy from.
     */
    ModernThreadSafeStack(const ModernThreadSafeStack& otherStack) {
        std::lock_guard<std::mutex> lock(otherStack.stackMutex_);
        internalStack_ = otherStack.internalStack_;
    }

    /**
     * @brief Assignment operator is disabled.
     *
     * Prevents accidental copying that could lead to unsafe behavior.
     */
    ModernThreadSafeStack& operator=(const ModernThreadSafeStack&) = delete;

    /**
     * @brief Move constructor.
     *
     * Transfers ownership of the internal stack from another instance
     * while safely locking the source object.
     *
     * @param otherStack Stack to move from.
     */
    ModernThreadSafeStack(ModernThreadSafeStack&& otherStack) noexcept {
        std::lock_guard<std::mutex> lock(otherStack.stackMutex_);
        internalStack_ = std::move(otherStack.internalStack_);
    }

    /**
     * @brief Destructor.
     */
    ~ModernThreadSafeStack() = default;

    /**
     * @brief Pushes a new element onto the stack.
     *
     * The operation is protected by a mutex to ensure thread safety.
     *
     * @param newElement Element to insert into the stack.
     */
    void push(ElementType newElement) {
        std::lock_guard<std::mutex> lock(stackMutex_);
        internalStack_.push(std::move(newElement));
    }

    /**
     * @brief Removes and returns the top element.
     *
     * @return Shared pointer to the removed element.
     * @throws std::out_of_range if the stack is empty.
     */
    std::shared_ptr<ElementType> pop() {
        std::lock_guard<std::mutex> lock(stackMutex_);
        if (internalStack_.empty()) {
            throw std::out_of_range("ModernThreadSafeStack::pop(): stack is empty");
        }

        auto topElement = std::make_shared<ElementType>(
            std::move(internalStack_.top())
        );
        internalStack_.pop();
        return topElement;
    }

    /**
     * @brief Removes the top element and stores it in the provided reference.
     *
     * @param outputReference Reference where the popped value will be stored.
     * @throws std::out_of_range if the stack is empty.
     */
    void pop(ElementType& outputReference) {
        std::lock_guard<std::mutex> lock(stackMutex_);
        if (internalStack_.empty()) {
            throw std::out_of_range("ModernThreadSafeStack::pop(reference): stack is empty");
        }

        outputReference = std::move(internalStack_.top());
        internalStack_.pop();
    }

    /**
     * @brief Attempts to pop the top element without throwing exceptions.
     *
     * @param outputReference Reference where the popped value will be stored.
     * @return true if an element was successfully removed, false otherwise.
     */
    bool tryPop(ElementType& outputReference) noexcept {
        try {
            std::lock_guard<std::mutex> lock(stackMutex_);
            if (internalStack_.empty()) {
                return false;
            }

            outputReference = std::move(internalStack_.top());
            internalStack_.pop();
            return true;
        }
        catch (...) {
            return false;
        }
    }

    /**
     * @brief Attempts to pop the top element and return it as a shared pointer.
     *
     * @return Shared pointer to the removed element, or nullptr if the stack is empty.
     */
    std::shared_ptr<ElementType> tryPop() noexcept {
        try {
            std::lock_guard<std::mutex> lock(stackMutex_);
            if (internalStack_.empty()) {
                return nullptr;
            }

            auto topElement = std::make_shared<ElementType>(
                std::move(internalStack_.top())
            );
            internalStack_.pop();
            return topElement;
        }
        catch (...) {
            return nullptr;
        }
    }

    /**
     * @brief Checks whether the stack is empty.
     *
     * @return true if the stack contains no elements.
     */
    bool isEmpty() const {
        std::lock_guard<std::mutex> lock(stackMutex_);
        return internalStack_.empty();
    }

    /**
     * @brief Returns the number of elements in the stack.
     *
     * @return Current stack size.
     */
    size_t getSize() const {
        std::lock_guard<std::mutex> lock(stackMutex_);
        return internalStack_.size();
    }

    /**
     * @brief Removes all elements from the stack.
     *
     * The operation is performed under mutex protection.
     */
    void clear() {
        std::lock_guard<std::mutex> lock(stackMutex_);
        while (!internalStack_.empty()) {
            internalStack_.pop();
        }
    }
};