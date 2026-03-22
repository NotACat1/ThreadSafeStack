#pragma once

#include "IStack.hpp"
#include <memory>
#include <mutex>
#include <stdexcept>

/**
 * @brief A thread-safe LIFO stack implementation based on a linked list.
 * * This class implements the IStack interface. It uses std::unique_ptr for node
 * ownership to ensure automatic memory management and std::shared_ptr for
 * element storage to allow safe data sharing. The implementation is thread-safe,
 * using a mutex to synchronize access to the underlying data structure.
 * * @tparam ElementType The type of elements stored in the stack.
 */
template<typename ElementType>
class LinkedStack : public IStack<ElementType> {
private:
    /**
     * @brief Internal node structure representing an entry in the stack.
     */
    struct Node {
        std::shared_ptr<ElementType> data;
        std::unique_ptr<Node> next;

        explicit Node(std::shared_ptr<ElementType> dataValue)
            : data(std::move(dataValue)), next(nullptr) {
        }
    };

    /// Pointer to the top of the stack (the head of the linked list).
    std::unique_ptr<Node> head_;

    /// Mutex protecting the head pointer and the size counter.
    mutable std::mutex stackMutex_;

    /// Current number of elements in the stack.
    size_t size_ = 0;

    /**
     * @brief Internal helper to safely remove the top node from the stack.
     * * Centralizes the logic for updating the head and decrementing the size
     * while holding the mutex.
     * * @return A unique pointer to the removed node, or nullptr if the stack is empty.
     */
    std::unique_ptr<Node> popHead() {
        std::lock_guard<std::mutex> lock(stackMutex_);
        if (!head_) {
            return nullptr;
        }

        std::unique_ptr<Node> oldHead = std::move(head_);
        head_ = std::move(oldHead->next);
        --size_;
        return oldHead;
    }

    /**
     * @brief Internal helper to encapsulate the logic of pushing a new node.
     * * Performs the pointer redirection and size increment within a critical section.
     * * @param data A shared pointer containing the element to be pushed.
     */
    void pushNode(std::shared_ptr<ElementType> data) {
        auto newNode = std::make_unique<Node>(std::move(data));
        std::lock_guard<std::mutex> lock(stackMutex_);
        newNode->next = std::move(head_);
        head_ = std::move(newNode);
        ++size_;
    }

public:
    /**
     * @brief Default constructor.
     */
    LinkedStack() = default;

    /**
     * @brief Copying is disabled to avoid expensive synchronization and complex
     * deep-copy logic in a concurrent context.
     */
    LinkedStack(const LinkedStack&) = delete;
    LinkedStack& operator=(const LinkedStack&) = delete;

    /**
     * @brief Destructor.
     * * Explicitly calls clear() to perform an iterative cleanup. This is necessary
     * for linked lists using std::unique_ptr to prevent a stack overflow
     * (due to recursive destruction) when deleting a very deep stack.
     */
    ~LinkedStack() override {
        clear();
    }

    // --- IStack Interface Implementation ---

    /**
     * @brief Pushes an element onto the stack by copying.
     * * Allocation of the shared_ptr and the Node happens before acquiring the lock
     * to reduce the duration of the critical section.
     * * @param value The value to be copied onto the stack.
     */
    void push(const ElementType& value) override {
        pushNode(std::make_shared<ElementType>(value));
    }

    /**
     * @brief Pushes an element onto the stack by moving.
     * * Efficiently transfers ownership of the provided value into the stack.
     * * @param value The rvalue to be moved onto the stack.
     */
    void push(ElementType&& value) override {
        pushNode(std::make_shared<ElementType>(std::move(value)));
    }

    /**
     * @brief Removes the top element and returns it.
     * * @return A shared pointer to the popped element.
     * @throws std::runtime_error if the stack is empty.
     */
    std::shared_ptr<ElementType> pop() override {
        std::unique_ptr<Node> oldHead = popHead();
        if (!oldHead) {
            throw std::runtime_error("Stack is empty");
        }
        return oldHead->data;
    }

    /**
     * @brief Attempts to pop an element into the provided reference.
     * * @param value Reference where the popped value will be moved.
     * @return true if an element was successfully popped; false if the stack was empty.
     */
    bool tryPop(ElementType& value) override {
        std::unique_ptr<Node> oldHead = popHead();
        if (!oldHead) {
            return false;
        }
        value = std::move(*oldHead->data);
        return true;
    }

    /**
     * @brief Attempts to pop an element and return it.
     * * Useful for types that are expensive to copy or when null check logic is preferred.
     * * @return A shared pointer to the data, or nullptr if the stack is empty.
     */
    std::shared_ptr<ElementType> tryPop() override {
        std::unique_ptr<Node> oldHead = popHead();
        return oldHead ? oldHead->data : nullptr;
    }

    /**
     * @brief Thread-safe check for stack emptiness.
     * * @return true if the stack contains no elements.
     */
    bool isEmpty() const override {
        std::lock_guard<std::mutex> lock(stackMutex_);
        return head_ == nullptr;
    }

    /**
     * @brief Returns the current number of elements in the stack.
     * * @return The size of the stack as a size_t.
     */
    size_t getSize() const override {
        std::lock_guard<std::mutex> lock(stackMutex_);
        return size_;
    }

    /**
     * @brief Iteratively clears the stack.
     * * Ensures all elements are destroyed safely without causing deep recursion.
     */
    void clear() override {
        std::lock_guard<std::mutex> lock(stackMutex_);
        while (head_) {
            std::unique_ptr<Node> oldHead = std::move(head_);
            head_ = std::move(oldHead->next);
        }
        size_ = 0;
    }
};