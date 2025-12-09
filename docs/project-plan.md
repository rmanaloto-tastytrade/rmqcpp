# Project Plan (current priorities)

- **Top priority:** Migrate `tastyworks/order-status-monitor` to use the rmqcpp AMQP library (see `docs/order-status-monitor-rmqcpp-migration.md` for analysis/plan).
- Add latency instrumentation for `order-status-monitor` (socket/kernel timestamps, in-process timing, cycle counters, OTel export). See `docs/order-status-monitor-latency.md`.
- Secondary: Modern C++26 RabbitMQ admin client (rabbitmqadmin replacement, HTTP-only CLI distinct from the AMQP rmqcpp libraries). See `docs/rabbitmqadmin-modern.md`.
- Telemetry options survey (OTel baseline; optional Tracy/Perfetto; HW counters Linux-only): see `docs/telemetry-options.md`.
- Enforce clang-22 + libc++ toolchain across presets; keep CMake/libc++ checks green.
- Continue monitor example hardening (single-threaded Asio loop, management polling TBD).
- **Match rabbitmqadmin-ng behavior (monitor/admin CLI):**
  - Extend HTTP crawl coverage to include connections/channels/consumers, policies/limits/feature-flags, etc., caching results alongside exchanges/queues/bindings.
  - Keep bindings as a single global `/api/bindings` call (no pagination flags); add a fallback to `/api/definitions` for bindings if `/api/bindings` fails, while logging errors.
  - Tune BALL log noise (category thresholds) while still forwarding all events to Quill.
  - Add config toggles for page_size/stats flags (default: no pagination to mirror rabbitmqadmin-ng; allow queue totals/stats to be disabled for speed).
  - Drive consumer creation from crawl results with filters for exclusive/auto-delete queues and optional whitelists.
- Documentation maintenance: ensure new admin client and monitor docs are linked from README/docs once stabilized.
- For every admin feature and test, use `rabbitmqadmin-ng` as the behavioral reference: if it works there, our `rmqadmin` should match.
