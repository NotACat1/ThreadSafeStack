#pragma once

#include <memory>
#include <mutex>
#include <stdexcept>

/**
 * @brief A thread-safe LIFO stack implementation using a custom linked list.
 * * This class uses a linked-list approach where each node is managed by
 * std::unique_ptr for ownership and std::shared_ptr for data storage.
 * It is designed for high-concurrency environments by minimizing the time
 * spent inside critical sections (e.g., performing allocations outside locks).
 * * @tparam ElementType Type of elements stored in the stack.
 */
template<typename ElementType>
class ModernThreadSafeLinkedStack {
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

    /**
     * @brief Internal helper to remove the top node from the stack.
     * * Centralizing this logic ensures consistency across different pop methods.
     * * @return A unique pointer to the removed node, or nullptr if the stack is empty.
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
    ModernThreadSafeLinkedStack() = default;

    /**
     * @brief Copying is disabled to prevent complex synchronization and performance overhead.
     */
    ModernThreadSafeLinkedStack(const ModernThreadSafeLinkedStack&) = delete;
    ModernThreadSafeLinkedStack& operator=(const ModernThreadSafeLinkedStack&) = delete;

    /**
     * @brief Destructor.
     * * Performs an iterative cleanup of the stack nodes. This is crucial for
     * linked lists using std::unique_ptr to prevent a stack overflow
     * (recursive destruction) when deleting a stack with many elements.
     */
    ~ModernThreadSafeLinkedStack() {
        clear();
    }

    /**
     * @brief Pushes a new element onto the stack.
     * * Memory allocation for the data and the node occurs outside the mutex lock
     * to maximize throughput and reduce thread contention.
     * * @param newValue The value to be moved or copied onto the stack.
     */
    void push(ElementType newValue) {
        // Allocate data and node before acquiring the lock
        auto data = std::make_shared<ElementType>(std::move(newValue));
        auto newNode = std::make_unique<Node>(data);

        std::lock_guard<std::mutex> lock(stackMutex_);
        newNode->next = std::move(head_);
        head_ = std::move(newNode);
    }

    /**
     * @brief Attempts to pop an element into the provided reference.
     * * @param value Reference where the popped value will be stored.
     * @return true if an element was successfully popped, false if the stack was empty.
     */
    bool tryPop(ElementType& value) {
        std::unique_ptr<Node> oldHead = popHead();
        if (!oldHead) {
            return false;
        }
        value = std::move(*oldHead->data);
        return true;
    }

    /**
     * @brief Attempts to pop an element and return it via a shared pointer.
     * * This version is useful when the type is expensive to copy or when
     * you want to handle the empty case via a null check.
     * * @return A shared pointer to the data, or nullptr if the stack is empty.
     */
    std::shared_ptr<ElementType> tryPop() {
        std::unique_ptr<Node> oldHead = popHead();
        return oldHead ? oldHead->data : std::shared_ptr<ElementType>();
    }

    /**
     * @brief Checks whether the stack is empty.
     * * @return true if the stack contains no nodes.
     */
    bool isEmpty() const {
        std::lock_guard<std::mutex> lock(stackMutex_);
        return head_ == nullptr;
    }

    /**
     * @brief Removes all elements from the stack.
     * * Uses iterative deletion to safely clear large stacks without recursion.
     */
    void clear() {
        std::lock_guard<std::mutex> lock(stackMutex_);
        while (head_) {
            std::unique_ptr<Node> oldHead = std::move(head_);
            head_ = std::move(oldHead->next);
        }
    }
};