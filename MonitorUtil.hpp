#pragma once

#include <cstdint>
#include <chrono>
#include <atomic>

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
#if defined(__APPLE__)
    static std::atomic<bool> init{false};
    static mach_timebase_info_data_t info{};
    if (!init.load(std::memory_order_acquire)) {
        mach_timebase_info(&info);
        init.store(true, std::memory_order_release);
    }
    const auto t = mach_absolute_time();
    return (t * info.numer) / (info.denom ? info.denom : 1);
#elif defined(__linux__)
    timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    return static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ull +
           static_cast<std::uint64_t>(ts.tv_nsec);
#else
    return static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
#endif
}

}  // namespace TW
