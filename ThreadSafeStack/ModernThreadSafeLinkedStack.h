#pragma once

#include <boost/pool/pool_alloc.hpp>
#include <boost/pool/singleton_pool.hpp>
#include <memory>
#include <mutex>
#include <stdexcept>

/**
 * @brief A non-blocking, thread-safe LIFO stack implemented as a singly linked list.
 *
 * This container provides immediate-return push and pop operations suitable
 * for high-throughput concurrent scenarios where blocking semantics are not
 * required.
 *
 * Design characteristics:
 * - Coarse-grained locking using a single mutex.
 * - Minimal time spent inside critical sections.
 * - Safe handling of large or move-only element types.
 * - Deterministic, non-recursive destruction.
 *
 * Memory management:
 * - Nodes are allocated from a Boost singleton memory pool.
 * - Payloads are stored in std::shared_ptr to enable efficient extraction
 *   without copying while holding the lock.
 * - Ownership of nodes is represented by std::unique_ptr with a custom
 *   pool-aware deleter.
 *
 * Thread-safety:
 * - All structural operations are serialized by a mutex.
 * - No lock-free guarantees are provided.
 *
 * @tparam ElementType Type of elements stored in the stack.
 */
template<typename ElementType>
class ModernThreadSafeLinkedStack {
private:
    /**
     * @brief Node representing a stack element.
     *
     * Forms part of a singly linked list. Each node stores:
     * - A shared pointer to the element payload
     * - A raw pointer to the next node
     *
     * The raw pointer enables external lifetime control via the pool
     * and prevents deep recursive destruction.
     */
    struct Node {
        std::shared_ptr<ElementType> data;
        Node* next;

        explicit Node(std::shared_ptr<ElementType> dataValue)
            : data(std::move(dataValue)), next(nullptr) {
        }
    };

    /// Tag type used to define a dedicated Boost pool instance.
    struct NodePoolTag {};

    /// Thread-safe global pool for Node allocations.
    using NodePool = boost::singleton_pool<NodePoolTag, sizeof(Node)>;

    /**
     * @brief Pool-aware deleter for nodes.
     *
     * Ensures proper destruction and memory reclamation when a node
     * is released by a smart pointer.
     */
    struct PoolDeleter {
        void operator()(Node* ptr) const {
            if (ptr) {
                ptr->~Node();
                NodePool::free(ptr);
            }
        }
    };

    /// Owning smart pointer type for nodes.
    using NodePtr = std::unique_ptr<Node, PoolDeleter>;

    /// Pointer to the top of the stack.
    NodePtr head_;

    /**
     * @brief Mutex protecting the internal linked list.
     *
     * Required for all operations that modify or read the structure.
     */
    mutable std::mutex stackMutex_;

    /**
     * @brief Removes the top node from the stack.
     *
     * This helper centralizes the core pointer manipulation used by
     * various pop operations, ensuring consistent behavior.
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
        auto data = std::allocate_shared<ElementType>(boost::fast_pool_allocator<ElementType>(), std::move(newValue));
        
        void* nodeMem = NodePool::malloc();
        if (!nodeMem) throw std::bad_alloc();

        NodePtr newNode(new (nodeMem) Node(data));

        std::lock_guard<std::mutex> lock(stackMutex_);
        newNode->next = head_.release();
        head_ = std::move(newNode);
    }

    /**
     * @brief Attempts to pop an element into the provided reference.
     * * @param value Reference where the popped value will be stored.
     * @return true if an element was successfully popped, false if the stack was empty.
     */
    bool tryPop(ElementType& value) {
        NodePtr oldHead = popHead();
        if (!oldHead) return false;
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
        NodePtr oldHead = popHead();
        return oldHead ? std::move(oldHead->data) : std::shared_ptr<ElementType>();
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
            NodePtr oldHead = std::move(head_);
            head_ = NodePtr(oldHead->next);
        }
    }
};