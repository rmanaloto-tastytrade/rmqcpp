# Project Plan (current priorities)

- **Top priority:** Migrate `tastyworks/order-status-monitor` to use the rmqcpp AMQP library (see `docs/order-status-monitor-rmqcpp-migration.md` for analysis/plan).
- Add latency instrumentation for `order-status-monitor` (socket/kernel timestamps, in-process timing, cycle counters, OTel export). See `docs/order-status-monitor-latency.md`.
- Secondary: Modern C++26 RabbitMQ admin client (rabbitmqadmin replacement, HTTP-only CLI distinct from the AMQP rmqcpp libraries). See `docs/rabbitmqadmin-modern.md`.
- Telemetry options survey (OTel baseline; optional Tracy/Perfetto; HW counters Linux-only): see `docs/telemetry-options.md`.
- Enforce clang-22 + libc++ toolchain across presets; keep CMake/libc++ checks green.
- Continue monitor example hardening (single-threaded Asio loop, management polling TBD).
- Documentation maintenance: ensure new admin client and monitor docs are linked from README/docs once stabilized.
- For every admin feature and test, use `rabbitmqadmin-ng` as the behavioral reference: if it works there, our `rmqadmin` should match.
