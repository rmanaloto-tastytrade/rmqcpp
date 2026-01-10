#pragma once

#include <cstdint>
#include <chrono>
#include <atomic>
#include <ctime>

#if defined(__APPLE__)
#    include <mach/mach_time.h>
#elif defined(__x86_64__) || defined(_M_X64)
#    include <x86intrin.h>
#elif defined(__aarch64__)
#    include <arm_neon.h>
#endif

namespace TW {

// Return a fast, monotonic-ish 64-bit counter.
// - macOS: mach_absolute_time() ticks (monotonic).
// - x86_64: RDTSCP/RDTSC (invariant TSC on modern CPUs).
// - AArch64: cntvct_el0 (virtual counter) if available.
// - Fallback: steady_clock ticks.
inline std::uint64_t getTSC()
{
#if defined(__APPLE__)
    return mach_absolute_time();
#elif defined(__x86_64__) || defined(_M_X64)
#    if defined(__RDTSCP__)
    unsigned aux{};
    return __rdtscp(&aux);
#    else
    return __rdtsc();
#    endif
#elif defined(__aarch64__)
    std::uint64_t val{};
#    if defined(__has_builtin) && __has_builtin(__builtin_readcyclecounter)
    val = __builtin_readcyclecounter();
#    else
    asm volatile("mrs %0, cntvct_el0" : "=r"(val));
#    endif
    return val;
#else
    return static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
#endif
}

inline std::uint64_t getRealtimeNs()
{
#if defined(CLOCK_REALTIME)
    timespec ts{};
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        return static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ull +
               static_cast<std::uint64_t>(ts.tv_nsec);
    }
#endif
    return static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
}

}  // namespace TW
