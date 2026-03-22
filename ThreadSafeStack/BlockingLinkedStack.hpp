#pragma once

#include "IWaitingStack.hpp"
#include <memory>
#include <mutex>
#include <condition_variable>
#include <stdexcept>
#include <chrono>

/**
 * @brief A thread-safe LIFO linked stack with blocking and timed-wait capabilities.
 * * This class implements the IWaitingStack interface using a linked-list structure.
 * It provides thread-safe operations for concurrent environments where consumers
 * may need to wait for data to be produced.
 * * @tparam ElementType The type of elements stored in the stack.
 */
template<typename ElementType>
class BlockingLinkedStack : public IWaitingStack<ElementType> {
private:
    /**
     * @brief Internal node structure for the linked list.
     */
    struct Node {
        std::shared_ptr<ElementType> data;
        std::unique_ptr<Node> next;

        explicit Node(std::shared_ptr<ElementType> dataValue)
            : data(std::move(dataValue)), next(nullptr) {
        }
    };

    std::unique_ptr<Node> head_;             ///< Pointer to the top of the stack
    mutable std::mutex stackMutex_;         ///< Mutex protecting all internal state
    std::condition_variable dataCondition_;  ///< Condition variable to signal waiting threads
    size_t size_ = 0;                       ///< Current number of elements in the stack

    /**
     * @brief Internal helper to remove the head node without blocking.
     * @return A unique_ptr to the removed node, or nullptr if the stack is empty.
     */
    std::unique_ptr<Node> popHead() {
        std::lock_guard<std::mutex> lock(stackMutex_);
        if (!head_) return nullptr;

        std::unique_ptr<Node> oldHead = std::move(head_);
        head_ = std::move(oldHead->next);
        --size_;
        return oldHead;
    }

    /**
     * @brief Internal helper to encapsulate the logic of adding a new node.
     * Handles locking, size incrementing, and signaling waiting threads.
     * @param dataValue Shared pointer to the data to be stored.
     */
    void pushNode(std::shared_ptr<ElementType> dataValue) {
        auto newNode = std::make_unique<Node>(std::move(dataValue));
        {
            std::lock_guard<std::mutex> lock(stackMutex_);
            newNode->next = std::move(head_);
            head_ = std::move(newNode);
            ++size_;
        }
        dataCondition_.notify_one();
    }

public:
    BlockingLinkedStack() = default;

    // Deleted copy operations to ensure thread-safety and unique ownership
    BlockingLinkedStack(const BlockingLinkedStack&) = delete;
    BlockingLinkedStack& operator=(const BlockingLinkedStack&) = delete;

    /**
     * @brief Destructor ensures all nodes are cleared to prevent deep recursion during cleanup.
     */
    ~BlockingLinkedStack() override {
        clear();
    }

    // --- IStack Implementation ---

    /**
     * @brief Pushes a copy of the value onto the stack.
     * @param value The value to copy and push.
     */
    void push(const ElementType& value) override {
        pushNode(std::make_shared<ElementType>(value));
    }

    /**
     * @brief Pushes the value onto the stack using move semantics.
     * @param value The value to move and push.
     */
    void push(ElementType&& value) override {
        pushNode(std::make_shared<ElementType>(std::move(value)));
    }

    /**
     * @brief Non-blocking pop.
     * @return Shared pointer to the popped element.
     * @throws std::runtime_error If the stack is empty.
     */
    std::shared_ptr<ElementType> pop() override {
        std::unique_ptr<Node> oldHead = popHead();
        if (!oldHead) {
            throw std::runtime_error("Stack is empty");
        }
        return oldHead->data;
    }

    /**
     * @brief Attempts to pop an element without blocking.
     * @param value Reference to store the popped value if successful.
     * @return true if an element was popped, false otherwise.
     */
    bool tryPop(ElementType& value) override {
        std::unique_ptr<Node> oldHead = popHead();
        if (!oldHead) return false;
        value = std::move(*oldHead->data);
        return true;
    }

    /**
     * @brief Attempts to pop an element without blocking.
     * @return Shared pointer to the element, or nullptr if empty.
     */
    std::shared_ptr<ElementType> tryPop() override {
        std::unique_ptr<Node> oldHead = popHead();
        return oldHead ? oldHead->data : nullptr;
    }

    /**
     * @brief Checks if the stack is empty.
     */
    bool isEmpty() const override {
        std::lock_guard<std::mutex> lock(stackMutex_);
        return head_ == nullptr;
    }

    /**
     * @brief Returns the current number of elements in the stack.
     */
    size_t getSize() const override {
        std::lock_guard<std::mutex> lock(stackMutex_);
        return size_;
    }

    /**
     * @brief Removes all elements from the stack.
     */
    void clear() override {
        std::lock_guard<std::mutex> lock(stackMutex_);
        while (head_) {
            std::unique_ptr<Node> oldHead = std::move(head_);
            head_ = std::move(oldHead->next);
        }
        size_ = 0;
    }

    // --- IWaitingStack Implementation ---

    /**
     * @brief Blocks until an element is available and pops it into the provided reference.
     * @param value Reference to store the popped value.
     */
    void waitAndPop(ElementType& value) override {
        std::unique_lock<std::mutex> lock(stackMutex_);
        dataCondition_.wait(lock, [this] { return head_ != nullptr; });

        std::unique_ptr<Node> oldHead = std::move(head_);
        head_ = std::move(oldHead->next);
        --size_;

        value = std::move(*oldHead->data);
    }

    /**
     * @brief Blocks until an element is available and returns it as a shared pointer.
     * @return Shared pointer to the popped element.
     */
    std::shared_ptr<ElementType> waitAndPop() override {
        std::unique_lock<std::mutex> lock(stackMutex_);
        dataCondition_.wait(lock, [this] { return head_ != nullptr; });

        std::unique_ptr<Node> oldHead = std::move(head_);
        head_ = std::move(oldHead->next);
        --size_;

        return oldHead->data;
    }

    /**
     * @brief Blocks for a limited time until an element is available.
     * @param value Reference to store the popped value.
     * @param timeout Maximum duration to wait.
     * @return true if an element was popped before the timeout, false otherwise.
     */
    bool waitAndPopFor(ElementType& value, const std::chrono::milliseconds& timeout) override {
        std::unique_lock<std::mutex> lock(stackMutex_);
        if (!dataCondition_.wait_for(lock, timeout, [this] { return head_ != nullptr; })) {
            return false;
        }

        std::unique_ptr<Node> oldHead = std::move(head_);
        head_ = std::move(oldHead->next);
        --size_;

        value = std::move(*oldHead->data);
        return true;
    }
};