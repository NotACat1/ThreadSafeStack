#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <string>
#include <iomanip>
#include <algorithm>

/**
 * Include your stack implementations here.
 * Ensure filenames match your actual header files.
 */
#include "ModernThreadSafeStack.h"
#include "WaitingThreadSafeStack.h"
#include "SharedPtrWaitingThreadSafeStack.h"
#include "ModernThreadSafeLinkedStack.h"
#include "ModernThreadSafeWaitingLinkedStack.h"

 // --- ANSI TERMINAL COLORS FOR FORMATTING ---
const std::string RESET   = "\033[0m";
const std::string BOLD    = "\033[1m";
const std::string CYAN    = "\033[36m";
const std::string GREEN   = "\033[32m";
const std::string YELLOW  = "\033[33m";
const std::string MAGENTA = "\033[35m";
const std::string RED     = "\033[31m";

/**
 * @brief Represents a large data object to test copy overhead.
 * 1KB payload forces the stack to deal with memory throughput bottlenecks.
 */
struct HeavyPayload {
    int data[256];
    HeavyPayload() { std::fill(std::begin(data), std::end(data), 0); }
};

using LightPayload = int;

/**
 * @brief Helper to print professional table headers.
 */
void printTableFrame() {
    std::cout << std::setfill('-') << std::setw(96) << "-" << std::setfill(' ') << "\n";
}

void printHeader(const std::string& title) {
    std::cout << "\n" << BOLD << MAGENTA << ">>> TEST CATEGORY: " << title << RESET << "\n";
    printTableFrame();
    std::cout << std::left << std::setw(37) << CYAN << "Implementation" << RESET
        << std::right << std::setw(17) << "Load (P/C)"
        << std::setw(13) << "Time (ms)"
        << std::setw(20) << "Throughput (MOps/s)" << "\n";
    printTableFrame();
}

/**
 * @brief Benchmark for non-blocking operations using spinning/yielding.
 * * Each consumer is assigned a fixed workload to prevent deadlocks and
 * minimize atomic contention on a global counter during the measurement.
 */
template <typename StackType, typename PayloadType>
void runTryPopBenchmark(const std::string& label, int numP, int numC, int itemsPerP) {
    StackType stack;
    const int totalItems = numP * itemsPerP;
    const int itemsPerC = totalItems / numC; // Workload per consumer

    std::atomic<bool> startSignal{ false };
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    // Initialize Consumer threads
    for (int i = 0; i < numC; ++i) {
        consumers.emplace_back([&, itemsPerC]() {
            while (!startSignal.load(std::memory_order_acquire)) {} // Spin-wait for start
            PayloadType item;
            for (int j = 0; j < itemsPerC; ++j) {
                // Spin with yield until an item is successfully retrieved
                while (!stack.tryPop(item)) {
                    std::this_thread::yield();
                }
            }
            });
    }

    // Initialize Producer threads
    for (int i = 0; i < numP; ++i) {
        producers.emplace_back([&, itemsPerP]() {
            while (!startSignal.load(std::memory_order_acquire)) {}
            for (int j = 0; j < itemsPerP; ++j) {
                stack.push(PayloadType{});
            }
            });
    }

    // Capture start time and trigger all threads simultaneously
    auto startTime = std::chrono::high_resolution_clock::now();
    startSignal.store(true, std::memory_order_release);

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = endTime - startTime;

    // Throughput = Total Operations (Push + Pop) / Time
    double mops = (totalItems * 2.0) / (elapsed.count() / 1000.0) / 1000000.0;

    std::cout << std::left << std::setw(40) << (YELLOW + label + RESET)
        << std::right << std::setw(7) << numP << "/" << std::left << std::setw(7) << numC
        << std::right << std::setw(15) << std::fixed << std::setprecision(2) << elapsed.count()
        << std::right << std::setw(22) << std::setprecision(3) << GREEN << mops << RESET << "\n";
}

/**
 * @brief Benchmark for blocking operations using condition variables.
 */
template <typename StackType, typename PayloadType>
void runWaitPopBenchmark(const std::string& label, int numP, int numC, int itemsPerP) {
    StackType stack;
    const int totalItems = numP * itemsPerP;
    const int itemsPerC = totalItems / numC;

    std::atomic<bool> startSignal{ false };
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    for (int i = 0; i < numC; ++i) {
        consumers.emplace_back([&, itemsPerC]() {
            while (!startSignal.load(std::memory_order_acquire)) {}
            PayloadType item;
            for (int j = 0; j < itemsPerC; ++j) {
                stack.waitAndPop(item); // Block until data is available
            }
            });
    }

    for (int i = 0; i < numP; ++i) {
        producers.emplace_back([&, itemsPerP]() {
            while (!startSignal.load(std::memory_order_acquire)) {}
            for (int j = 0; j < itemsPerP; ++j) {
                stack.push(PayloadType{});
            }
            });
    }

    auto startTime = std::chrono::high_resolution_clock::now();
    startSignal.store(true, std::memory_order_release);

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = endTime - startTime;

    double mops = (totalItems * 2.0) / (elapsed.count() / 1000.0) / 1000000.0;

    std::cout << std::left << std::setw(40) << (CYAN + label + RESET)
        << std::right << std::setw(7) << numP << "/" << std::left << std::setw(7) << numC
        << std::right << std::setw(15) << std::fixed << std::setprecision(2) << elapsed.count()
        << std::right << std::setw(22) << std::setprecision(3) << GREEN << mops << RESET << "\n";
}

int main() {
    // Benchmark configuration
    const int itemsPerProducer = 100000;
    const int P = 4; // Number of producers
    const int C = 4; // Number of consumers

    // --- TEST PHASE 1: LIGHTWEIGHT DATA (INT) ---
    printHeader("LIGHT PAYLOAD (INT) - CONCURRENCY OVERHEAD");

    runTryPopBenchmark<ModernThreadSafeStack<LightPayload>, LightPayload>("Basic Mutex Stack", P, C, itemsPerProducer);
    runTryPopBenchmark<WaitingThreadSafeStack<LightPayload>, LightPayload>("Waiting Mutex Stack", P, C, itemsPerProducer);
    runTryPopBenchmark<SharedPtrWaitingThreadSafeStack<LightPayload>, LightPayload>("SharedPtr Stack", P, C, itemsPerProducer);
    runTryPopBenchmark<ModernThreadSafeLinkedStack<LightPayload>, LightPayload>("Linked List Stack", P, C, itemsPerProducer);
    runTryPopBenchmark<ModernThreadSafeWaitingLinkedStack<LightPayload>, LightPayload>("Wait Linked List Stack", P, C, itemsPerProducer);

    printTableFrame();
    runWaitPopBenchmark<WaitingThreadSafeStack<LightPayload>, LightPayload>("Waiting Mutex Stack [WAIT]", P, C, itemsPerProducer);
    runWaitPopBenchmark<SharedPtrWaitingThreadSafeStack<LightPayload>, LightPayload>("SharedPtr Stack [WAIT]", P, C, itemsPerProducer);
    runWaitPopBenchmark<ModernThreadSafeWaitingLinkedStack<LightPayload>, LightPayload>("Wait Linked List Stack [WAIT]", P, C, itemsPerProducer);
    printTableFrame();

    // --- TEST PHASE 2: HEAVYWEIGHT DATA (1KB) ---
    printHeader("HEAVY PAYLOAD (1KB) - MEMORY & COPY OVERHEAD");

    runTryPopBenchmark<ModernThreadSafeStack<HeavyPayload>, HeavyPayload>("Basic Mutex Stack", P, C, itemsPerProducer);
    runTryPopBenchmark<WaitingThreadSafeStack<HeavyPayload>, HeavyPayload>("Waiting Mutex Stack", P, C, itemsPerProducer);
    runTryPopBenchmark<SharedPtrWaitingThreadSafeStack<HeavyPayload>, HeavyPayload>("SharedPtr Stack", P, C, itemsPerProducer);
    runTryPopBenchmark<ModernThreadSafeLinkedStack<HeavyPayload>, HeavyPayload>("Linked List Stack", P, C, itemsPerProducer);
    runTryPopBenchmark<ModernThreadSafeWaitingLinkedStack<HeavyPayload>, HeavyPayload>("Wait Linked List Stack", P, C, itemsPerProducer);

    printTableFrame();
    runWaitPopBenchmark<WaitingThreadSafeStack<HeavyPayload>, HeavyPayload>("Waiting Mutex Stack [WAIT]", P, C, itemsPerProducer);
    runWaitPopBenchmark<SharedPtrWaitingThreadSafeStack<HeavyPayload>, HeavyPayload>("SharedPtr Stack [WAIT]", P, C, itemsPerProducer);
    runWaitPopBenchmark<ModernThreadSafeWaitingLinkedStack<HeavyPayload>, HeavyPayload>("Wait Linked List Stack [WAIT]", P, C, itemsPerProducer);
    printTableFrame();

    std::cout << "\n" << BOLD << GREEN << "Benchmark complete." << RESET << "\n";
    return 0;
}