# Telemetry & Profiling Options (Linux primary, macOS dev)

## Baseline: OpenTelemetry (already wired)
- Purpose: always-on traces/metrics for latency/health.
- Status: In tree; supports OTLP (grpc/http) and file (NDJSON) fallback.
- Action: emit per-message latency spans/metrics when ready.

## Optional profilers/tracers

### Tracy (vcpkg: `tracy`)
- Pros: lightweight instrumentation (zones), nanosecond timestamps, live UI; good on Linux/macOS for user-space profiling.
- Cons: requires running Tracy server; hardware counters limited; mostly user-space timing on macOS.
- macOS hardware counters: Tracy does not expose perf counters on macOS; relies on timers.
- Why consider: great for dev profiling sessions; easy opt-in via CMake flag.

### Perfetto (vcpkg: `perfetto`)
- Pros: powerful tracing (kernel + user on Linux), ftrace integration, rich timeline UI.
- Cons: heavier SDK; macOS lacks kernel perf counters support; user-space tracing works, but hardware counters not available on macOS.
- Why consider: deep Linux tracing and correlation with kernel events.

## Hardware counters (Linux vs macOS)

### Linux
- Options: PAPI/libpfm (not in vcpkg), perf_event_open (syscall) for HW counters; fits well with Perfetto (ftrace) if integrated manually.
- Current vcpkg: no PAPI/libpfm port; would need overlay or custom integration.

### macOS
- Hardware counters: limited. No perf_event_open; PAPI support is sparse. You can sample via `mach_absolute_time` (monotonic ticks) and possibly `sysctl`-exposed counters, but full PMU access is restricted.
- Conclusion: expect user-space timing only on macOS; treat hardware counters as Linux-only feature.

## Recommendation
1. Keep OTel as primary (always-on) for latency/health (both platforms).
2. Add optional Tracy instrumentation behind a CMake flag for dev profiling (user-space timing on macOS, more on Linux).
3. Add optional Perfetto integration behind a CMake flag for deep Linux tracing; macOS limited to user-space traces.
4. For hardware counters, plan a Linux-only path using perf_event_open or PAPI/libpfm (custom overlay), with macOS falling back to timers.

## Notes
- vcpkg search results: `perfetto`, `tracy` available; no PAPI/libpfm ports; `lexbor[perf]` mentions rdtsc but is unrelated.
- Abseil/Google Benchmark bring cycle clocks but are heavier than a small in-tree helper.

## Hardware counter research
- **Linux**: Invariant TSC on modern x86_64 is stable across cores/sockets; use RDTSCP for ordered reads. For PMU events, perf_event_open (or PAPI/libpfm overlays) is the path; NUMA/multi-socket systems can have skewed TSC on very old CPUs, but current hardware keeps TSC synchronized. Use rdtscp/steady_clock if unsure.
- **macOS (Apple Silicon)**: PMU access is restricted; no perf_event_open. Expect user-space clocks only (mach_absolute_time). No direct access to cycle counters without entitlements.
- **AArch64/Linux**: CNTVCT_EL0 virtual counter is typically enabled; read via `mrs %0, cntvct_el0` or `__builtin_readcyclecounter` if available.
- **Libraries reviewed**: Tracy, Perfetto (available in vcpkg); qlibs/perf (local, x86/Linux-focused, not in vcpkg); no small rdtsc wrapper in vcpkg. Conclusion: keep a small in-tree helper for cycle reads and rely on OTel/Tracy/Perfetto for higher-level instrumentation.
