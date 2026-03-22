#pragma once

#include "IStack.hpp"
#include <chrono>

/**
 * @brief Extension of IStack that provides blocking operations.
 *
 * This interface is intended for stacks that support waiting semantics,
 * allowing threads to block until data becomes available.
 */
template <typename T>
class IWaitingStack : public IStack<T> {
public:
    /**
     * @brief Blocks until an element is available, then pops it into `value`.
     *
     * @param value Reference where the popped value will be stored.
     */
    virtual void waitAndPop(T& value) = 0;

    /**
     * @brief Blocks until an element is available, then returns it.
     *
     * @return Shared pointer to the removed element.
     */
    [[nodiscard]] virtual std::shared_ptr<T> waitAndPop() = 0;

    /**
     * @brief Waits for an element to become available up to a specified timeout.
     *
     * @param value Reference where the popped value will be stored.
     * @param timeout Maximum time to wait.
     * @return true if an element was successfully popped, false if the timeout expired.
     *
     * @note Template methods cannot be virtual in C++, therefore the timeout
     * type is fixed to std::chrono::milliseconds.
     */
    [[nodiscard]] virtual bool waitAndPopFor(
        T& value,
        const std::chrono::milliseconds& timeout
    ) = 0;
};