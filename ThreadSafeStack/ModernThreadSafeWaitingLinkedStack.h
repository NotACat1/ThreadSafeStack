#pragma once

#include <boost/pool/pool_alloc.hpp>
#include <boost/pool/singleton_pool.hpp>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <stdexcept>
#include <chrono>

/**
 * @brief A blocking, thread-safe LIFO stack implemented as a singly linked list.
 *
 * This container supports concurrent access by multiple producer and consumer
 * threads and provides blocking as well as timed waiting pop operations.
 *
 * Design objectives:
 * - Minimize lock contention by performing memory allocation outside
 *   critical sections.
 * - Provide strong exception safety for push/pop operations.
 * - Avoid recursive destruction for deep stacks.
 * - Enable efficient transfer of large or non-trivially copyable objects.
 *
 * Concurrency model:
 * - All structural modifications are protected by a single mutex.
 * - Consumer threads may block on a condition variable until data becomes
 *   available.
 * - Spurious wakeups are handled via predicate-based waiting.
 *
 * Memory management:
 * - Nodes are allocated from a thread-safe Boost singleton memory pool.
 * - Element payloads are stored in std::shared_ptr to allow extraction
 *   without copying while holding the mutex.
 * - Node ownership is represented by std::unique_ptr with a custom deleter
 *   returning memory to the pool.
 *
 * @tparam ElementType Type of elements stored in the stack.
 */
template<typename ElementType>
class ModernThreadSafeWaitingLinkedStack {
private:
    /**
     * @brief Node representing a single stack entry.
     *
     * The stack is implemented as a singly linked list where each node
     * contains:
     * - A shared pointer to the stored element
     * - A raw pointer to the next node
     *
     * The use of a raw pointer avoids recursive destruction chains that
     * would occur with nested unique_ptr ownership and enables external
     * memory pool management.
     */
    struct Node {
        std::shared_ptr<ElementType> data;
        Node* next;

        explicit Node(std::shared_ptr<ElementType> dataValue)
            : data(std::move(dataValue)), next(nullptr) {
        }
    };

    /// Tag type used to create a unique Boost singleton memory pool.
    struct NodePoolTag {};

    /**
     * @brief Thread-safe global memory pool for Node allocations.
     *
     * singleton_pool guarantees that all allocations of this type share
     * a single pool instance across threads, reducing allocation overhead
     * and fragmentation.
     */
    using NodePool = boost::singleton_pool<NodePoolTag, sizeof(Node)>;

    /**
     * @brief Custom deleter that returns nodes to the pool.
     *
     * Ensures that:
     * - The Node destructor is invoked explicitly
     * - Memory is released back to the Boost pool
     *
     * This is required because nodes are allocated using placement new.
     */
    struct PoolDeleter {
        void operator()(Node* ptr) const {
            if (ptr) {
                ptr->~Node();          // Destroy contained objects
                NodePool::free(ptr);   // Return memory to pool
            }
        }
    };

    /// Owning smart pointer type for nodes.
    using NodePtr = std::unique_ptr<Node, PoolDeleter>;

    /// Pointer to the current top node (nullptr if empty).
    NodePtr head_;

    /**
     * @brief Mutex protecting the internal linked list.
     *
     * All push and pop operations must hold this mutex while modifying
     * the stack structure.
     */
    mutable std::mutex stackMutex_;

    /**
     * @brief Condition variable used to signal availability of data.
     *
     * Producers notify waiting consumers when a new element is pushed.
     */
    std::condition_variable dataCondition_;

    /**
     * @brief Removes the top node in a thread-safe manner.
     *
     * This function acquires the mutex, detaches the head node,
     * and transfers ownership to the caller.
     *
     * @return Unique ownership of the removed node, or nullptr if empty.
     */
    NodePtr popHead() {
        std::lock_guard<std::mutex> lock(stackMutex_);

        if (!head_) {
            return nullptr;
        }

        NodePtr oldHead = std::move(head_);
        head_ = NodePtr(oldHead->next);
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
        auto data = std::allocate_shared<ElementType>(
            boost::fast_pool_allocator<ElementType>(),
            std::move(newValue)
        );

        void* nodeMem = NodePool::malloc();
        if (!nodeMem) throw std::bad_alloc();
        NodePtr newNode(new (nodeMem) Node(std::move(data)));

        {
            std::lock_guard<std::mutex> lock(stackMutex_);
            newNode->next = head_.release();
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

        NodePtr oldHead = std::move(head_);
        head_ = NodePtr(oldHead->next);

        outputReference = std::move(*oldHead->data);
    }

    /**
     * @brief Blocks until an element is available and returns it.
     * * @return Shared pointer to the popped element.
     */
    std::shared_ptr<ElementType> waitAndPop() {
        std::unique_lock<std::mutex> lock(stackMutex_);
        dataCondition_.wait(lock, [this] { return head_ != nullptr; });

        NodePtr oldHead = std::move(head_);
        head_ = NodePtr(oldHead->next);

        return std::move(oldHead->data);
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

        NodePtr oldHead = std::move(head_);
        head_ = NodePtr(oldHead->next);

        outputReference = std::move(*oldHead->data);
        return true;
    }

    /**
     * @brief Non-blocking attempt to pop an element into a reference.
     * * @param outputReference Variable receiving the value.
     * @return true if successful.
     */
    bool tryPop(ElementType& outputReference) {
        NodePtr oldHead = popHead();
        if (!oldHead) return false;

        outputReference = std::move(*oldHead->data);
        return true;
    }

    /**
     * @brief Non-blocking attempt to pop an element as a shared_ptr.
     * * @return Shared pointer to data, or nullptr if empty.
     */
    std::shared_ptr<ElementType> tryPop() {
        NodePtr oldHead = popHead();
        return oldHead ? std::move(oldHead->data) : std::shared_ptr<ElementType>();
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
            NodePtr oldHead = std::move(head_);
            head_ = NodePtr(oldHead->next);
        }
    }
};