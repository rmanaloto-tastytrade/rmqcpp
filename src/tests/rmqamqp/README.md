# rmqamqp Tests

Focus on AMQP channel and connection state machines. Target links pull `rmqtestutil`, `rmqamqp`, `rmqt`, and transitive dependencies to assert real framing behavior rather than stubs.

## Cases
- `rmqamqp_channel.t.cpp` / `rmqamqp_channeltests.t.cpp` / `rmqamqp_channelmap.t.cpp` — channel lifecycle, mapping, and debug helpers.
- `rmqamqp_connection.t.cpp` — connection handshake, channel allocation, and error handling.
- `rmqamqp_contentmaker.t.cpp` — payload framing.
- `rmqamqp_framer.t.cpp` — frame serialization/deserialization against `rmqamqpt`.
- `rmqamqp_heartbeatmanagerimpl.t.cpp` — heartbeat scheduling and detection.
- `rmqamqp_messagestore.t.cpp` — buffering/delivery ordering.
- `rmqamqp_multipleackhandler.t.cpp` — bulk confirms.
- `rmqamqp_receivechannel.t.cpp` / `rmqamqp_sendchannel.t.cpp` — consumer/publisher channel behaviors.
- `rmqamqp_topologytransformer.t.cpp` / `rmqamqp_topologymerger.t.cpp` — mapping declarative topology to AMQP methods.

## Notes
- Links to `rmqt` because tests validate conversion between protocol frames and typed data.
- Uses `rmqtestutil` mocks to control timers/resolvers, keeping the event loop deterministic for single-threaded `io_context` testing.
