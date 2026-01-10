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
- **Libraries reviewed**: Tracy, Perfetto (available in vcpkg); qlibs/perf (local, x86/Linux-focused, header-only, could be added via overlay later), no small rdtsc wrapper in vcpkg. Conclusion: keep a small in-tree helper for cycle reads and rely on OTel/Tracy/Perfetto for higher-level instrumentation; revisit qlibs/perf as an optional Linux-only PMU helper via overlay if we need deeper counters.

## Alignment with Perfetto/Tracy timestamp sources


### Perfetto
- The Perfetto SDK timestamps events using its base clock helpers (e.g., base::GetBootTimeNs()/GetWallTimeNs) backed by platform monotonic clocks. There is no public API to fetch raw hardware counters; timestamps are generated internally when you emit trace events.
- Matching Perfetto: use the same monotonic sources we already do—on Linux x86 use rdtscp for fast monotonically increasing counter if available; otherwise the monotonic clock. On macOS, Perfetto uses mach_absolute_time() under the hood for monotonic time.
- Conclusion: there is no stable public "get hardware counter" API in Perfetto. Align by using the same clock sources (monotonic/rdtscp) we already use.


### Tracy
- Tracy internally uses rdtsc/rdtscp on x86 and fallbacks to platform clocks on other architectures. It does not expose a supported public API to fetch the counter; the intended use is to instrument zones and let Tracy timestamp them.
- Matching Tracy: use the same logic—rdtscp on x86 when available; monotonic clocks on other platforms. Our TW::getTSC mirrors this behavior.
- Conclusion: there is no clean public accessor to Tracy's timestamp source; matching their logic is the best way to stay in sync.


### Summary
- Neither Perfetto nor Tracy provide a public API to hand out their internal timestamp source. Both rely on platform monotonic clocks (Perfetto) or rdtsc/rdtscp plus clocks (Tracy).
- Our TW::getTSC matches these sources: rdtscp on x86, cntvct_el0 on AArch64, mach_absolute_time on macOS, steady_clock fallback.
- If we integrate Perfetto/Tracy later, we should keep TW::getTSC aligned to the same platform clocks rather than trying to call into their internals.

## Aggregating per-message latency by exchange/queue/binding (minimal overhead)
- **Primary (out-of-process) path**: OpenTelemetry metrics/traces.
  - Emit a histogram metric per binding/queue/exchange (labels: `queue`, `exchange`, optionally `binding_key`) with the handler duration (ns). OTLP export to a collector gives min/max/avg/p50/p99 outside the process.
  - Emit span events or attributes with the same labels if traces are desired.
  - Overhead is low if using a histogram and batching; can be toggled off via config.
- **Optional profiling**:
  - Tracy: add zones around handler; Tracy server shows min/max/avg per zone. Good for dev profiling; not ideal for always-on prod.
  - Perfetto: add trace events around handler; can aggregate in Perfetto UI for p50/p99. Heavier, more suited to deep profiling.
- **In-process vs out-of-process**:
  - Prefer out-of-process aggregation (OTel collector) to keep overhead down and allow long-lived stats (min/max/avg/rolling).
  - In-process rolling stats (min/max/avg/EMA) are easy to add but risk memory/CPU overhead at high volume; keep optional.
- **Libraries/ports in vcpkg**:
  - OpenTelemetry C++ (already wired) for histograms/spans.
  - Tracy (vcpkg `tracy`) for dev profiling; requires running the Tracy server.
  - Perfetto (vcpkg `perfetto`) for deep Linux tracing; heavier and better suited for profiling sessions.



## Data flow & visualization options
- **OTel (primary):** Emit histograms/spans for latency (queue/exchange/binding labels) to an OTLP collector. Use Grafana/Tempo/Jaeger/etc. for real-time dashboards.
- **Arrow/Sparrow (optional offline store):** Buffer per-message samples and flush to Arrow IPC/Feather (vcpkg `arrow`) or a modern C++20 implementation like Sparrow (to be evaluated). Good for offline columnar analysis; heavier than OTel. Guarded by config.
- **Perfetto (profiling):** Optional tracing for Linux (kernel+user) to inspect timelines; user-space only on macOS. Real-time viewing via Perfetto UI.
- **Tracy (profiling):** Optional zones around handler for dev profiling; real-time viewing via Tracy server. Minimal code changes; not for always-on prod.
- **Visualization (CLI/TUI):** Consider ftxui or imgui to present live stats/flight data from collected metrics/traces, if a local viewer is desired.

## Planned data points & workflow (high level)
- **Socket arrival timestamp** (Linux-only via SO_TIMESTAMPING/SCM_TIMESTAMPING) → stored with message metadata.
- **Handler entry/exit timestamps** (current) via CLOCK_REALTIME and TSC.
- **Per-message record:** {queue, exchange, binding, duration_ns, tsc_entry, tsc_exit, real_entry_ns, real_exit_ns, optional socket_ts_ns}.
- **Export paths:**
  - OTel histograms/spans (primary, out-of-process aggregation).
  - Optional Arrow/Feather or Sparrow flush for offline analysis.
  - Optional Perfetto/Tracy instrumentation for profiling sessions.
- **Real-time view:** OTel dashboards (Grafana/etc.); optional TUI (ftxui/imgui) for local quick looks.

## Notes on Arrow/Sparrow
- vcpkg provides `arrow`; Sparrow (modern C++20 Arrow-like impl) needs evaluation/availability. Use Arrow IPC/Feather for columnar dumps if enabled.
