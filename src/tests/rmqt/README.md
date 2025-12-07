# rmqt Tests

Covers the shared data types. `rmqt_tests` only link `rmqt` and its transitive libs plus GTest, keeping these fast and dependency-light.

## Cases
- `rmqt_consumerconfig.t.cpp` — prefetch and ack configuration defaults.
- `rmqt_envelope.t.cpp` — envelope population on delivery.
- `rmqt_exchange.t.cpp` — exchange handles and types.
- `rmqt_fieldvalue.t.cpp` — AMQP field table conversions.
- `rmqt_future.t.cpp` — future/promise behavior used by the event loop.
- `rmqt_message.t.cpp` — message payload/property helpers.
- `rmqt_plaincredentials.t.cpp` — credential validation.
- `rmqt_secureendpoint.t.cpp`, `rmqt_simpleendpoint.t.cpp` — endpoint validation and TLS flags.

## Notes
- Targets link fewer libraries because the types are pure data; higher-level protocol code is covered elsewhere.
