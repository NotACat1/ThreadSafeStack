#pragma once

#include <stack>
#include <mutex>
#include <condition_variable>
#include <stdexcept>
#include <memory>
#include <chrono>

/**
 * @brief A thread-safe stack with blocking and timed waiting capabilities.
 *
 * This class provides a synchronized wrapper around std::stack and supports
 * multiple concurrency patterns:
 *
 * - Blocking pop operations that wait until data becomes available.
 * - Timed waiting operations with configurable timeouts.
 * - Non-blocking operations that return immediately.
 * - Exception-based operations for strict error handling.
 *
 * A condition variable is used to allow threads to efficiently wait for
 * new elements to be pushed onto the stack.
 *
 * @tparam ElementType Type of elements stored in the stack.
 */
template<typename ElementType>
class WaitingThreadSafeStack {
private:

    /// Underlying container storing stack elements
    std::stack<ElementType> internalStack_;

    /// Mutex protecting access to the stack
    mutable std::mutex stackMutex_;

    /// Condition variable used to notify waiting threads about new data
    std::condition_variable dataCondition_;

public:

    /**
     * @brief Default constructor.
     */
    WaitingThreadSafeStack() = default;

    /**
     * @brief Copy constructor.
     *
     * Performs a thread-safe copy of another stack instance.
     * The source stack is locked during the copy operation.
     *
     * @param otherStack Stack to copy from.
     */
    WaitingThreadSafeStack(const WaitingThreadSafeStack& otherStack) {
        std::lock_guard<std::mutex> lock(otherStack.stackMutex_);
        internalStack_ = otherStack.internalStack_;
    }

    /**
     * @brief Assignment operator is disabled.
     *
     * Prevents unsafe copying that could violate thread-safety guarantees.
     */
    WaitingThreadSafeStack& operator=(const WaitingThreadSafeStack&) = delete;

    /**
     * @brief Move constructor.
     *
     * Transfers ownership of the underlying stack while safely
     * locking the source object.
     *
     * @param otherStack Stack to move from.
     */
    WaitingThreadSafeStack(WaitingThreadSafeStack&& otherStack) noexcept {
        std::lock_guard<std::mutex> lock(otherStack.stackMutex_);
        internalStack_ = std::move(otherStack.internalStack_);
    }

    /**
     * @brief Destructor.
     */
    ~WaitingThreadSafeStack() = default;

    /**
     * @brief Pushes a new element onto the stack.
     *
     * After insertion, one waiting thread is notified that new data
     * is available.
     *
     * @param newElement Element to be inserted.
     */
    void push(ElementType newElement) {
        {
            std::lock_guard<std::mutex> lock(stackMutex_);
            internalStack_.push(std::move(newElement));
        }
        dataCondition_.notify_one();
    }

    /**
     * @brief Pushes multiple elements onto the stack.
     *
     * All waiting threads are notified after the elements are inserted.
     *
     * @param elements List of elements to insert.
     */
    void push_multiple(std::initializer_list<ElementType> elements) {
        {
            std::lock_guard<std::mutex> lock(stackMutex_);
            for (auto&& elem : elements) {
                internalStack_.push(std::move(elem));
            }
        }
        dataCondition_.notify_all();
    }

    /**
     * @brief Waits until an element becomes available and removes it.
     *
     * This function blocks the calling thread until the stack
     * contains at least one element.
     *
     * @param outputReference Reference where the popped value will be stored.
     */
    void waitAndPop(ElementType& outputReference) {
        std::unique_lock<std::mutex> lock(stackMutex_);
        dataCondition_.wait(lock, [this] {
            return !internalStack_.empty();
            });

        outputReference = std::move(internalStack_.top());
        internalStack_.pop();
    }

    /**
     * @brief Waits until an element becomes available and returns it.
     *
     * @return Shared pointer to the popped element.
     */
    std::shared_ptr<ElementType> waitAndPop() {
        std::unique_lock<std::mutex> lock(stackMutex_);
        dataCondition_.wait(lock, [this] {
            return !internalStack_.empty();
            });

        auto topElement = std::make_shared<ElementType>(
            std::move(internalStack_.top())
        );
        internalStack_.pop();
        return topElement;
    }

    /**
     * @brief Waits for an element with a timeout and pops it if available.
     *
     * @param outputReference Reference where the popped value will be stored.
     * @param timeout Maximum waiting duration.
     * @return true if an element was popped before timeout expired.
     */
    bool waitAndPopFor(ElementType& outputReference,
        const std::chrono::milliseconds& timeout) {

        std::unique_lock<std::mutex> lock(stackMutex_);
        if (!dataCondition_.wait_for(lock, timeout, [this] {
            return !internalStack_.empty();
            })) {
            return false;
        }

        outputReference = std::move(internalStack_.top());
        internalStack_.pop();
        return true;
    }

    /**
     * @brief Waits for an element with timeout and returns it.
     *
     * @param timeout Maximum waiting duration.
     * @return Shared pointer to the element, or nullptr if timeout expires.
     */
    std::shared_ptr<ElementType> waitAndPopFor(
        const std::chrono::milliseconds& timeout) {

        std::unique_lock<std::mutex> lock(stackMutex_);
        if (!dataCondition_.wait_for(lock, timeout, [this] {
            return !internalStack_.empty();
            })) {
            return nullptr;
        }

        auto topElement = std::make_shared<ElementType>(
            std::move(internalStack_.top())
        );
        internalStack_.pop();
        return topElement;
    }

    /**
     * @brief Attempts to pop the top element without blocking.
     *
     * @return Shared pointer to the element, or nullptr if the stack is empty.
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
     * @brief Attempts to pop the top element without blocking.
     *
     * @param outputReference Reference where the popped value will be stored.
     * @return true if an element was successfully removed.
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
     * @brief Removes and returns the top element.
     *
     * @throws std::out_of_range if the stack is empty.
     */
    std::shared_ptr<ElementType> pop() {
        std::lock_guard<std::mutex> lock(stackMutex_);
        if (internalStack_.empty()) {
            throw std::out_of_range("WaitingThreadSafeStack::pop(): stack is empty");
        }

        auto topElement = std::make_shared<ElementType>(
            std::move(internalStack_.top())
        );
        internalStack_.pop();
        return topElement;
    }

    /**
     * @brief Removes the top element and stores it in a reference.
     *
     * @param outputReference Reference where the popped value will be stored.
     * @throws std::out_of_range if the stack is empty.
     */
    void pop(ElementType& outputReference) {
        std::lock_guard<std::mutex> lock(stackMutex_);
        if (internalStack_.empty()) {
            throw std::out_of_range("WaitingThreadSafeStack::pop(reference): stack is empty");
        }

        outputReference = std::move(internalStack_.top());
        internalStack_.pop();
    }

    /**
     * @brief Returns a reference to the top element.
     *
     * @throws std::out_of_range if the stack is empty.
     */
    ElementType& top() {
        std::lock_guard<std::mutex> lock(stackMutex_);
        if (internalStack_.empty()) {
            throw std::out_of_range("WaitingThreadSafeStack::top(): stack is empty");
        }
        return internalStack_.top();
    }

    /**
     * @brief Returns a const reference to the top element.
     *
     * @throws std::out_of_range if the stack is empty.
     */
    const ElementType& top() const {
        std::lock_guard<std::mutex> lock(stackMutex_);
        if (internalStack_.empty()) {
            throw std::out_of_range("WaitingThreadSafeStack::top() const: stack is empty");
        }
        return internalStack_.top();
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
     * @brief Returns the number of elements currently stored in the stack.
     *
     * @return Current stack size.
     */
    size_t getSize() const {
        std::lock_guard<std::mutex> lock(stackMutex_);
        return internalStack_.size();
    }

    /**
     * @brief Removes all elements from the stack.
     */
    void clear() {
        std::lock_guard<std::mutex> lock(stackMutex_);
        while (!internalStack_.empty()) {
            internalStack_.pop();
        }
    }
};