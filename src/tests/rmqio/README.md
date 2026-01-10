# rmqio Tests

Validates the I/O layer and event loop behavior. `rmqio_tests` link in `rmqamqpt`, `rmqt`, `rmqio`, `rmqamqp`, transitive link libraries, and `rmqtestutil` so real framing and timers are exercised.

## Cases
- `rmqio_asioeventloop.t.cpp` — lifecycle of the `io_context`, work guard, and shutdown semantics.
- `rmqio_asioconnection.t.cpp` — socket read/write flows and error handling.
- `rmqio_asioresolver.t.cpp` — DNS resolution and endpoint shuffling.
- `rmqio_asiotimer.t.cpp` — timer scheduling accuracy.
- `rmqio_backofflevelretrystrategy.t.cpp`, `rmqio_retryhandler.t.cpp`, `rmqio_connectionretryhandler.t.cpp` — retry policies and rescheduling.
- `rmqio_decoder.t.cpp` — frame decoding with split buffers.
- `rmqio_eventloop.t.cpp` — `post`/`dispatch`/`postF` behavior and thread start.
- `rmqio_watchdog.t.cpp` — watchdog heartbeat and forced shutdown scenarios.

## Notes
- Emphasizes single-threaded event loop ordering, ensuring callbacks posted from other Asio services would interleave correctly when sharing the same `io_context`.
