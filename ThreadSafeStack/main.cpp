#include <benchmark/benchmark.h>
#include <memory>
#include <thread>
#include <vector>

/**
 * Подключаем ваши реализации стека.
 */
#include "MutexStack.hpp"
#include "BlockingStack .hpp"
#include "SharedStack.hpp"
#include "LinkedStack.hpp"
#include "BlockingLinkedStack.hpp"

 // --- ТЕСТОВЫЕ ДАННЫЕ ---

struct HeavyPayload {
    int data[256];
    HeavyPayload() { std::fill(std::begin(data), std::end(data), 0); }
};

using LightPayload = int;

// --- УНИВЕРСАЛЬНЫЙ ШАБЛОН ТЕСТА ---

/**
 * @tparam StackType Тип стека
 * @tparam Payload Тип данных (Light/Heavy)
 * @tparam UseWaitPop Если true — используем waitAndPop, иначе tryPop с yield
 */
template <typename StackType, typename Payload, bool UseWaitPop>
void BM_Stack_ProducerConsumer(benchmark::State& state) {
    // Используем static или shared_ptr, чтобы все потоки работали с одним экземпляром
    // Google Benchmark гарантирует, что Setup выполнится корректно
    static std::unique_ptr<StackType> shared_stack;

    if (state.thread_index() == 0) {
        shared_stack = std::make_unique<StackType>();
    }

    // Разделяем потоки: четные — Producers, нечетные — Consumers
    if (state.thread_index() % 2 == 0) {
        // PRODUCER
        for (auto _ : state) {
            shared_stack->push(Payload{});
        }
    }
    else {
        // CONSUMER
        Payload p;
        for (auto _ : state) {
            if constexpr (UseWaitPop) {
                shared_stack->waitAndPop(p);
            }
            else {
                while (!shared_stack->tryPop(p)) {
                    std::this_thread::yield();
                }
            }
        }
    }

    // Установка счетчика пропускной способности (Items per second)
    if (state.thread_index() == 0) {
        state.SetItemsProcessed(state.iterations() * state.threads());
    }
}

// --- РЕГИСТРАЦИЯ ТЕСТОВ ---

// Конфигурация: 8 потоков (4 Producer + 4 Consumer)
#define STACK_THREADS ->Threads(8)

// 1. Тесты для LIGHT PAYLOAD (int) - tryPop
BENCHMARK_TEMPLATE(BM_Stack_ProducerConsumer, MutexStack<int>, int, false)
->Name("TryPop/MutexStack/Light") STACK_THREADS;

BENCHMARK_TEMPLATE(BM_Stack_ProducerConsumer, LinkedStack<int>, int, false)
->Name("TryPop/LinkedStack/Light") STACK_THREADS;

// 2. Тесты для LIGHT PAYLOAD (int) - waitAndPop
BENCHMARK_TEMPLATE(BM_Stack_ProducerConsumer, BlockingStack<int>, int, true)
->Name("WaitPop/BlockingStack/Light") STACK_THREADS;

BENCHMARK_TEMPLATE(BM_Stack_ProducerConsumer, SharedStack<int>, int, true)
->Name("WaitPop/SharedStack/Light") STACK_THREADS;

BENCHMARK_TEMPLATE(BM_Stack_ProducerConsumer, BlockingLinkedStack<int>, int, true)
->Name("WaitPop/BlockingLinkedStack/Light") STACK_THREADS;

// 3. Тесты для HEAVY PAYLOAD (1KB) - tryPop
BENCHMARK_TEMPLATE(BM_Stack_ProducerConsumer, MutexStack<HeavyPayload>, HeavyPayload, false)
->Name("TryPop/MutexStack/Heavy") STACK_THREADS;

BENCHMARK_TEMPLATE(BM_Stack_ProducerConsumer, LinkedStack<HeavyPayload>, HeavyPayload, false)
->Name("TryPop/LinkedStack/Heavy") STACK_THREADS;

// 4. Тесты для HEAVY PAYLOAD (1KB) - waitAndPop
BENCHMARK_TEMPLATE(BM_Stack_ProducerConsumer, BlockingStack<HeavyPayload>, HeavyPayload, true)
->Name("WaitPop/BlockingStack/Heavy") STACK_THREADS;

BENCHMARK_TEMPLATE(BM_Stack_ProducerConsumer, SharedStack<HeavyPayload>, HeavyPayload, true)
->Name("WaitPop/SharedStack/Heavy") STACK_THREADS;

BENCHMARK_TEMPLATE(BM_Stack_ProducerConsumer, BlockingLinkedStack<HeavyPayload>, HeavyPayload, true)
->Name("WaitPop/BlockingLinkedStack/Heavy") STACK_THREADS;

// Макрос запуска Google Benchmark
BENCHMARK_MAIN();