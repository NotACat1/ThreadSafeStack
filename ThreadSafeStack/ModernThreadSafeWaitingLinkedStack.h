#pragma once

#include <memory>
#include <mutex>
#include <condition_variable>
#include <stdexcept>
#include <chrono>

/**
 * @brief A high-performance thread-safe LIFO stack using a linked-list structure.
 * * This implementation uses manual node management to allow for fine-grained
 * control over memory. By allocating nodes and data outside of critical
 * sections, it minimizes lock contention.
 * * Key Features:
 * - Iterative cleanup to prevent stack overflow on deep stacks.
 * - Blocking and timed-waiting pop operations.
 * - Shared pointer-based internal storage for exception safety and efficiency.
 * * @tparam ElementType Type of elements stored in the stack.
 */
template<typename ElementType>
class ModernThreadSafeWaitingLinkedStack {
private:
    /**
     * @brief Internal node representing an entry in the stack.
     */
    struct Node {
        std::shared_ptr<ElementType> data;
        std::unique_ptr<Node> next;

        explicit Node(std::shared_ptr<ElementType> dataValue)
            : data(std::move(dataValue)), next(nullptr) {
        }
    };

    /// Pointer to the top of the stack.
    std::unique_ptr<Node> head_;

    /// Mutex protecting the head_ pointer.
    mutable std::mutex stackMutex_;

    /// Condition variable to notify waiting threads when data is pushed.
    std::condition_variable dataCondition_;

    /**
     * @brief Internal helper to remove the top node from the stack.
     * @return A unique pointer to the removed node, or nullptr if empty.
     */
    std::unique_ptr<Node> popHead() {
        std::lock_guard<std::mutex> lock(stackMutex_);
        if (!head_) {
            return nullptr;
        }

        std::unique_ptr<Node> oldHead = std::move(head_);
        head_ = std::move(oldHead->next);
        return oldHead;
    }

public:
    /**
     * @brief Default constructor.
     */
    ModernThreadSafeWaitingLinkedStack() = default;

    /**
     * @brief Copying is disabled to prevent complex synchronization and overhead.
     */
    ModernThreadSafeWaitingLinkedStack(const ModernThreadSafeWaitingLinkedStack&) = delete;
    ModernThreadSafeWaitingLinkedStack& operator=(const ModernThreadSafeWaitingLinkedStack&) = delete;

    /**
     * @brief Destructor.
     * * Uses iterative cleanup to prevent recursive destruction of unique_ptr,
     * which can cause a stack overflow for very deep stacks.
     */
    ~ModernThreadSafeWaitingLinkedStack() {
        clear();
    }

    /**
     * @brief Pushes a new element onto the stack.
     * * Data allocation happens outside the lock to improve concurrency.
     * * @param newValue Value to be pushed.
     */
    void push(ElementType newValue) {
        auto data = std::make_shared<ElementType>(std::move(newValue));
        auto newNode = std::make_unique<Node>(data);

        {
            std::lock_guard<std::mutex> lock(stackMutex_);
            newNode->next = std::move(head_);
            head_ = std::move(newNode);
        }
        dataCondition_.notify_one();
    }

    /**
     * @brief Blocks until an element is available, then pops it.
     * * @param outputReference Variable receiving the popped value.
     */
    void waitAndPop(ElementType& outputReference) {
        std::unique_lock<std::mutex> lock(stackMutex_);
        dataCondition_.wait(lock, [this] { return head_ != nullptr; });

        std::unique_ptr<Node> oldHead = std::move(head_);
        head_ = std::move(oldHead->next);

        outputReference = std::move(*oldHead->data);
    }

    /**
     * @brief Blocks until an element is available and returns it.
     * * @return Shared pointer to the popped element.
     */
    std::shared_ptr<ElementType> waitAndPop() {
        std::unique_lock<std::mutex> lock(stackMutex_);
        dataCondition_.wait(lock, [this] { return head_ != nullptr; });

        std::unique_ptr<Node> oldHead = std::move(head_);
        head_ = std::move(oldHead->next);

        return oldHead->data;
    }

    /**
     * @brief Waits for an element with a timeout.
     * * @param outputReference Variable receiving the popped value.
     * @param timeout Maximum duration to wait.
     * @return true if an element was popped before the timeout.
     */
    bool waitAndPopFor(ElementType& outputReference, const std::chrono::milliseconds& timeout) {
        std::unique_lock<std::mutex> lock(stackMutex_);
        if (!dataCondition_.wait_for(lock, timeout, [this] { return head_ != nullptr; })) {
            return false;
        }

        std::unique_ptr<Node> oldHead = std::move(head_);
        head_ = std::move(oldHead->next);

        outputReference = std::move(*oldHead->data);
        return true;
    }

    /**
     * @brief Non-blocking attempt to pop an element into a reference.
     * * @param outputReference Variable receiving the value.
     * @return true if successful.
     */
    bool tryPop(ElementType& outputReference) {
        std::unique_ptr<Node> oldHead = popHead();
        if (!oldHead) return false;

        outputReference = std::move(*oldHead->data);
        return true;
    }

    /**
     * @brief Non-blocking attempt to pop an element as a shared_ptr.
     * * @return Shared pointer to data, or nullptr if empty.
     */
    std::shared_ptr<ElementType> tryPop() {
        std::unique_ptr<Node> oldHead = popHead();
        return oldHead ? oldHead->data : std::shared_ptr<ElementType>();
    }

    /**
     * @brief Checks if the stack is empty.
     */
    bool isEmpty() const {
        std::lock_guard<std::mutex> lock(stackMutex_);
        return head_ == nullptr;
    }

    /**
     * @brief Clears the stack using an iterative approach for safety.
     */
    void clear() {
        std::lock_guard<std::mutex> lock(stackMutex_);
        while (head_) {
            std::unique_ptr<Node> oldHead = std::move(head_);
            head_ = std::move(oldHead->next);
        }
    }
};