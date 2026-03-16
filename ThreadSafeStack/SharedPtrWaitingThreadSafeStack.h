#pragma once

#include <stack>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <chrono>

/**
 * @brief Thread-safe stack using shared_ptr-based storage.
 *
 * Unlike traditional implementations that store values directly,
 * this stack stores std::shared_ptr<T> internally. This approach
 * improves exception safety and avoids unnecessary copying of
 * large objects.
 *
 * Advantages of this design:
 *
 * - Reduced copying during push/pop operations
 * - Strong exception safety guarantees
 * - Better performance for large or non-trivial objects
 *
 * The stack supports:
 *
 * - Blocking pop operations
 * - Timed waiting pop operations
 * - Non-blocking tryPop operations
 *
 * @tparam ElementType Type of elements stored in the stack.
 */
template<typename ElementType>
class SharedPtrWaitingThreadSafeStack
{
private:

    /// Underlying container storing shared pointers to elements
    std::stack<std::shared_ptr<ElementType>> internalStack_;

    /// Mutex protecting access to the stack
    mutable std::mutex stackMutex_;

    /// Condition variable used to wake waiting threads
    std::condition_variable dataCondition_;

public:

    /**
     * @brief Default constructor.
     */
    SharedPtrWaitingThreadSafeStack() = default;

    /**
     * @brief Copy constructor.
     *
     * Performs a thread-safe copy of another stack instance.
     */
    SharedPtrWaitingThreadSafeStack(
        const SharedPtrWaitingThreadSafeStack& otherStack)
    {
        std::lock_guard<std::mutex> lock(otherStack.stackMutex_);
        internalStack_ = otherStack.internalStack_;
    }

    /**
     * @brief Copy assignment disabled to avoid unsafe behavior.
     */
    SharedPtrWaitingThreadSafeStack& operator=(
        const SharedPtrWaitingThreadSafeStack&) = delete;

    /**
     * @brief Move constructor.
     */
    SharedPtrWaitingThreadSafeStack(
        SharedPtrWaitingThreadSafeStack&& otherStack) noexcept
    {
        std::lock_guard<std::mutex> lock(otherStack.stackMutex_);
        internalStack_ = std::move(otherStack.internalStack_);
    }

    /**
     * @brief Destructor.
     */
    ~SharedPtrWaitingThreadSafeStack() = default;

    /**
     * @brief Pushes a new element onto the stack.
     *
     * The value is first wrapped into a shared_ptr before
     * acquiring the lock to minimize time spent inside
     * the critical section.
     *
     * @param newElement Element to insert.
     */
    void push(ElementType newElement)
    {
        std::shared_ptr<ElementType> data =
            std::make_shared<ElementType>(std::move(newElement));

        {
            std::lock_guard<std::mutex> lock(stackMutex_);
            internalStack_.push(data);
        }

        dataCondition_.notify_one();
    }

    /**
     * @brief Waits until an element is available and pops it.
     *
     * @param outputReference Variable receiving the popped value.
     */
    void waitAndPop(ElementType& outputReference)
    {
        std::unique_lock<std::mutex> lock(stackMutex_);

        dataCondition_.wait(lock, [this] {
            return !internalStack_.empty();
            });

        outputReference = std::move(*internalStack_.top());
        internalStack_.pop();
    }

    /**
     * @brief Waits until an element is available and returns it.
     *
     * @return Shared pointer to the popped element.
     */
    std::shared_ptr<ElementType> waitAndPop()
    {
        std::unique_lock<std::mutex> lock(stackMutex_);

        dataCondition_.wait(lock, [this] {
            return !internalStack_.empty();
            });

        auto result = internalStack_.top();
        internalStack_.pop();

        return result;
    }

    /**
     * @brief Attempts to pop an element without blocking.
     *
     * @param outputReference Variable receiving the popped value.
     * @return true if an element was popped.
     */
    bool tryPop(ElementType& outputReference)
    {
        std::lock_guard<std::mutex> lock(stackMutex_);

        if (internalStack_.empty())
            return false;

        outputReference = std::move(*internalStack_.top());
        internalStack_.pop();

        return true;
    }

    /**
     * @brief Attempts to pop an element without blocking.
     *
     * @return Shared pointer to the element, or nullptr if empty.
     */
    std::shared_ptr<ElementType> tryPop()
    {
        std::lock_guard<std::mutex> lock(stackMutex_);

        if (internalStack_.empty())
            return std::shared_ptr<ElementType>();

        auto result = internalStack_.top();
        internalStack_.pop();

        return result;
    }

    /**
     * @brief Waits for an element with timeout.
     *
     * @param outputReference Variable receiving the popped value.
     * @param timeout Maximum waiting time.
     * @return true if element was popped before timeout.
     */
    bool waitAndPopFor(
        ElementType& outputReference,
        const std::chrono::milliseconds& timeout)
    {
        std::unique_lock<std::mutex> lock(stackMutex_);

        if (!dataCondition_.wait_for(
            lock,
            timeout,
            [this] { return !internalStack_.empty(); }))
        {
            return false;
        }

        outputReference = std::move(*internalStack_.top());
        internalStack_.pop();

        return true;
    }

    /**
     * @brief Returns true if the stack contains no elements.
     */
    bool isEmpty() const
    {
        std::lock_guard<std::mutex> lock(stackMutex_);
        return internalStack_.empty();
    }

    /**
     * @brief Returns the number of stored elements.
     */
    size_t getSize() const
    {
        std::lock_guard<std::mutex> lock(stackMutex_);
        return internalStack_.size();
    }

    /**
     * @brief Removes all elements from the stack.
     */
    void clear()
    {
        std::lock_guard<std::mutex> lock(stackMutex_);

        while (!internalStack_.empty())
        {
            internalStack_.pop();
        }
    }
};