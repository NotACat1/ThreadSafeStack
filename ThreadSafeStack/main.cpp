#include <benchmark/benchmark.h>
#include <memory>
#include <thread>
#include <vector>

/**
 * @brief Include stack implementations under test.
 *
 * Each header provides a different synchronization strategy:
 * - Mutex-based
 * - Blocking (condition_variable)
 * - Shared ownership
 * - Linked-node based
 */
#include "MutexStack.hpp"
#include "BlockingStack.hpp"
#include "SharedStack.hpp"
#include "LinkedStack.hpp"
#include "BlockingLinkedStack.hpp"

 // --- TEST PAYLOAD TYPES ---

 /**
  * @brief Represents a heavy payload (~1 KB).
  *
  * Used to evaluate the cost of copying/moving large objects
  * and the effectiveness of memory management strategies.
  */
struct HeavyPayload {
    int data[256];

    HeavyPayload() {
        std::fill(std::begin(data), std::end(data), 0);
    }
};

/**
 * @brief Lightweight payload used for baseline performance measurements.
 */
using LightPayload = int;


// --- GENERIC BENCHMARK TEMPLATE ---

/**
 * @brief Producer-consumer benchmark for stack implementations.
 *
 * @tparam StackType  Stack implementation type.
 * @tparam Payload    Data type stored in the stack (light or heavy).
 * @tparam UseWaitPop If true, uses blocking pop (waitAndPop),
 *                    otherwise uses non-blocking tryPop with yielding.
 *
 * This benchmark simulates a typical concurrent workload where:
 * - Half of the threads act as producers (push operations)
 * - Half act as consumers (pop operations)
 *
 * The goal is to measure throughput and scalability under contention.
 */
template <typename StackType, typename Payload, bool UseWaitPop>
void BM_Stack_ProducerConsumer(benchmark::State& state) {
    /**
     * @brief Shared stack instance across all threads.
     *
     * Google Benchmark guarantees proper synchronization of setup
     * when using thread_index() == 0.
     */
    static std::unique_ptr<StackType> shared_stack;

    if (state.thread_index() == 0) {
        shared_stack = std::make_unique<StackType>();
    }

    /**
     * @brief Thread role assignment:
     * - Even threads  -> Producers
     * - Odd threads   -> Consumers
     */
    if (state.thread_index() % 2 == 0) {
        // --- PRODUCER ---
        for (auto _ : state) {
            shared_stack->push(Payload{});
        }
    }
    else {
        // --- CONSUMER ---
        Payload p;

        for (auto _ : state) {
            if constexpr (UseWaitPop) {
                /**
                 * Blocking variant:
                 * Thread sleeps until data becomes available.
                 */
                shared_stack->waitAndPop(p);
            }
            else {
                /**
                 * Non-blocking variant:
                 * Active waiting with cooperative yielding.
                 *
                 * This models lock-free or low-latency scenarios
                 * but may increase CPU usage.
                 */
                while (!shared_stack->tryPop(p)) {
                    std::this_thread::yield();
                }
            }
        }
    }

    /**
     * @brief Report total number of processed operations.
     *
     * Only one thread updates the counter to avoid contention.
     * The metric represents total push/pop operations performed.
     */
    if (state.thread_index() == 0) {
        state.SetItemsProcessed(state.iterations() * state.threads());
    }
}


// --- BENCHMARK REGISTRATION ---

/**
 * @brief Benchmark configuration:
 * 8 threads total (4 producers + 4 consumers).
 */
#define STACK_THREADS ->Threads(8)


 // --- LIGHT PAYLOAD (int) ---

 // Non-blocking (tryPop)
BENCHMARK_TEMPLATE(BM_Stack_ProducerConsumer, MutexStack<int>, int, false)
->Name("TryPop/MutexStack/Light") STACK_THREADS;

BENCHMARK_TEMPLATE(BM_Stack_ProducerConsumer, LinkedStack<int>, int, false)
->Name("TryPop/LinkedStack/Light") STACK_THREADS;

// Blocking (waitAndPop)
BENCHMARK_TEMPLATE(BM_Stack_ProducerConsumer, BlockingStack<int>, int, true)
->Name("WaitPop/BlockingStack/Light") STACK_THREADS;

BENCHMARK_TEMPLATE(BM_Stack_ProducerConsumer, SharedStack<int>, int, true)
->Name("WaitPop/SharedStack/Light") STACK_THREADS;

BENCHMARK_TEMPLATE(BM_Stack_ProducerConsumer, BlockingLinkedStack<int>, int, true)
->Name("WaitPop/BlockingLinkedStack/Light") STACK_THREADS;


// --- HEAVY PAYLOAD (~1 KB) ---

// Non-blocking (tryPop)
BENCHMARK_TEMPLATE(BM_Stack_ProducerConsumer, MutexStack<HeavyPayload>, HeavyPayload, false)
->Name("TryPop/MutexStack/Heavy") STACK_THREADS;

BENCHMARK_TEMPLATE(BM_Stack_ProducerConsumer, LinkedStack<HeavyPayload>, HeavyPayload, false)
->Name("TryPop/LinkedStack/Heavy") STACK_THREADS;

// Blocking (waitAndPop)
BENCHMARK_TEMPLATE(BM_Stack_ProducerConsumer, BlockingStack<HeavyPayload>, HeavyPayload, true)
->Name("WaitPop/BlockingStack/Heavy") STACK_THREADS;

BENCHMARK_TEMPLATE(BM_Stack_ProducerConsumer, SharedStack<HeavyPayload>, HeavyPayload, true)
->Name("WaitPop/SharedStack/Heavy") STACK_THREADS;

BENCHMARK_TEMPLATE(BM_Stack_ProducerConsumer, BlockingLinkedStack<HeavyPayload>, HeavyPayload, true)
->Name("WaitPop/BlockingLinkedStack/Heavy") STACK_THREADS;


/**
 * @brief Entry point for Google Benchmark.
 */
BENCHMARK_MAIN();