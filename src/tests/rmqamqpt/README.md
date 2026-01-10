# rmqamqpt Tests

Protocol-level serialization/roundtrip tests. `rmqamqpt_tests` links `rmqtestutil`, `rmqamqpt`, and `rmqamqp` (for shared helpers) plus transitive link libraries to validate binary encodings.

## Cases
- Frame encoding: `rmqamqpt_frame.t.cpp`, `rmqamqpt_buffer.t.cpp`, `rmqamqpt_types.t.cpp`, `rmqamqpt_fieldvalue.t.cpp`.
- Basic class: ack/cancel/consume/deliver/nack/properties/publish/qos/return roundtrips.
- Channel class: open/close/flow lifecycle.
- Connection class: start/tune/open/close negotiation.
- Confirm class: select/select-ok support.
- Exchange class: declare/bind and their ok replies.
- Queue class: declare/bind/delete/unbind and replies.

## Notes
- Tests ensure the encoded byte streams match the AMQP 0-9-1 spec and can be decoded back, protecting higher layers from wire-format regressions.
