#pragma once
#include <memory>
#include <cstddef>

/**
 * @brief Interface for a thread-safe stack.
 *
 * Provides a minimal and safe API for concurrent stack implementations.
 * The design avoids exposing operations that could lead to race conditions
 * (e.g., separating `empty()` and `pop()` in an unsafe way).
 */
template <typename T>
class IStack {
public:
    virtual ~IStack() = default;

    /**
     * @brief Pushes a value onto the stack (copy version).
     *
     * Accepts an lvalue reference to avoid unnecessary temporaries.
     */
    virtual void push(const T& value) = 0;

    /**
     * @brief Pushes a value onto the stack (move version).
     *
     * Enables efficient insertion of rvalues and move-only types.
     */
    virtual void push(T&& value) = 0;

    /**
     * @brief Removes and returns the top element of the stack.
     *
     * @return Shared pointer to the removed element.
     *
     * @throws EmptyStackException if the stack is empty (implementation-defined).
     *
     * Using std::shared_ptr ensures safe ownership transfer without
     * exposing references to internal storage.
     */
    virtual std::shared_ptr<T> pop() = 0;

    /**
     * @brief Attempts to remove the top element and store it in `value`.
     *
     * @param value Reference where the popped value will be stored.
     * @return true if an element was successfully popped, false if the stack was empty.
     *
     * Marked [[nodiscard]] to prevent ignoring the result of the operation.
     */
    [[nodiscard]] virtual bool tryPop(T& value) = 0;

    /**
     * @brief Attempts to remove the top element and return it.
     *
     * @return Shared pointer to the removed element, or nullptr if the stack is empty.
     *
     * This overload avoids the need for an output parameter.
     */
    [[nodiscard]] virtual std::shared_ptr<T> tryPop() = 0;

    /**
     * @brief Checks whether the stack is empty.
     *
     * @return true if the stack contains no elements.
     *
     * Note: In concurrent contexts, the result is only a snapshot
     * and may become outdated immediately after the call.
     */
    [[nodiscard]] virtual bool isEmpty() const = 0;

    /**
     * @brief Returns the number of elements in the stack.
     *
     * @return Current size of the stack.
     *
     * Note: Like isEmpty(), this value may be approximate in highly concurrent scenarios.
     */
    [[nodiscard]] virtual size_t getSize() const = 0;

    /**
     * @brief Removes all elements from the stack.
     *
     * The exact synchronization guarantees depend on the implementation.
     */
    virtual void clear() = 0;
};