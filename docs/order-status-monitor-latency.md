# Order Status Monitor: Latency Instrumentation Plan

Goal: capture per-message latency across three points and export to OpenTelemetry (spans/metrics), staying single-threaded in Boost.Asio.

## Target timestamps (per message)
1. **Socket receive timestamp** (kernel arrival, ideally from the NIC/kernel).
2. **rmqcpp receive start** (immediately before our message processing begins).
3. **Application handler done** (after our consumer callback finishes).

For each point, record:
- `timespec` via `clock_gettime(CLOCK_REALTIME)` (and optionally `CLOCK_MONOTONIC_RAW` for jitter analysis).
- Cycle counter: `rdtscp` on Linux/x86_64; Apple Silicon fallback via `mach_absolute_time()` (or `cntvct_el0` if allowed). Prefer a small inline helper; no heavy external deps.

## Socket timestamping (platform notes)
- **Linux**: enable kernel timestamps on the native socket:
  - `SO_TIMESTAMPNS` (legacy, nanoseconds) or `SO_TIMESTAMPING_NEW` with `SOF_TIMESTAMPING_RX_SOFTWARE` (and optionally RAW/HW flags if available).
  - Requires `recvmsg` to read control messages (`SCM_TIMESTAMPING(NS|NEW)`); plain `recv`/`read` will not carry the ancillary data.
  - Plan: use the native handle from the rmqcpp Asio socket, set the option once, and add a small `recvmsg` wrapper to extract the control message before handing the payload to rmqcpp. Keep the Asio event loop single-threaded.
- **macOS**: kernel TCP timestamping support is limited. `SO_TIMESTAMP` / `SO_TIMESTAMP_MONOTONIC` exist but are historically datagram-focused and return `timeval`. If kernel timestamps are not reliable, fall back to `clock_gettime` in user space at the read boundary. Document the limitation.
- **Boost.Asio integration**: not built-in. Use `socket.native_handle()` to call `setsockopt`, and a custom `recvmsg` path (via `asio::posix::stream_descriptor` or direct `::recvmsg` on the native handle) to capture control messages alongside the normal read buffer.

## Cycle counter helpers
- Linux/x86_64: use `__rdtscp` from `<x86intrin.h>`; handle serialization (read `IA32_TSC_AUX` out param).
- Apple Silicon: use `mach_absolute_time()` (monotonic ticks); consider reading `cntvct_el0` if allowed. Document that `rdtscp` is unavailable on ARM.
- Package needs: none beyond standard headers; if we want a small helper, consider a lightweight header-only utility (or just inline helpers in the CLI).

## Data flow & OTel export
- Add a per-message struct `{ socket_ts_timespec, socket_ts_cycles, recv_start_ts, recv_start_cycles, handler_done_ts, handler_done_cycles }`.
- Convert to OTel spans or metrics (histogram for latency; span events for the three points). Respect existing OTEL enable/disable flags.
- Include queue/exchange/vhost attributes on spans/metrics.

## Implementation outline
1. Add a configuration flag to enable kernel timestamping (Linux-only) and a fallback for user-space timestamping.
2. On connect, set socket options (`SO_TIMESTAMPING_NEW` or `SO_TIMESTAMPNS` on Linux; attempt `SO_TIMESTAMP_MONOTONIC` on macOS and log capability).
3. Replace/augment the read path with a `recvmsg` helper to extract the control message timestamp; store alongside the payload before rmqcpp processes it.
4. Stamp the two in-process times (receive start, handler done) using `clock_gettime` and cycle counter helper.
5. Emit to OTel spans/metrics; log to file for debugging when OTEL is disabled.

## Open questions / tasks
- Verify macOS TCP timestamping behavior; if unreliable, document and disable kernel timestamps there.
- Decide which clocks to keep (real-time vs. monotonic/raw) for cross-host comparison.
- Bound per-message overhead: consider feature flag to disable cycle-counter reads if too expensive.
- Tests: add unit/integration coverage for timestamp extraction on Linux (with a loopback broker) and a graceful fallback on macOS.
