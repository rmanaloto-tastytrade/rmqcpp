# rmqa Tests

C++ unit tests for the implementation layer. Target links: `rmqa_tests` links `rmqtestutil`, `rmqa`, `rmqt`, `rmqio`, `rmqtestmocks`, plus the transitive link libraries from those targets to exercise real protocol/IO code paths.

## Cases
- `rmqa_connectionimpl.t.cpp` — connection creation, reconnect workflow, use of `rmqio::EventLoop` and retry handlers.
- `rmqa_connectionmonitor.t.cpp` — watchdog/retry scheduling behavior.
- `rmqa_connectionstring.t.cpp` — URI parsing and generation for endpoints.
- `rmqa_consumerimpl.t.cpp` — consumer creation, cancel/redeclare paths, and ack/nack handling.
- `rmqa_messagetransformer.t.cpp` — outbound/inbound transformation hooks (compression/encryption shims).
- `rmqa_messageguard.t.cpp` — RAII ack/nack semantics on consumer callbacks.
- `rmqa_producerimpl.t.cpp` — publish/send buffering, confirm handling, backpressure.
- `rmqa_rabbitcontextimpl.t.cpp` — context lifecycle, event loop startup, and VHost creation.
- `rmqa_rabbitcontextoptions.t.cpp` — option defaults for event loop ownership, compression, tracing.
- `rmqa_topology.t.cpp` — queue/exchange/binding declaration helpers.
- `rmqa_vhostimpl.t.cpp` — vhost composition and producer/consumer factory behavior.

## Notes
- Tests run atop the actual `rmqio::AsioEventLoop` and protocol stack to ensure the single-threaded `io_context` path is exercised.
- Different link libraries mirror production layering: `rmqa` uses both `rmqamqp` and `rmqio` transitively, so the tests pull those in to validate integration, not just mocks.
