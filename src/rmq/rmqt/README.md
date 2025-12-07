# rmqt (Data Types)

Shared value types that flow between API (`rmqp`/`rmqa`), protocol (`rmqamqp`), and tests. Everything is plain data plus light helpers, making it easy to move across threads and futures.

## Connection & Security
- `Endpoint`, `SimpleEndpoint`, `SecureEndpoint` — host/port plus TLS knobs.
- `SecurityParameters`, `MutualSecurityParameters` — TLS certificate, key, and verification configuration.
- `Credentials`, `PlainCredentials` — username/password carriers.
- `VHostInfo` — bundle of endpoint + credentials for convenience.

## Messaging & Delivery
- `Message` — payload plus properties; holds shared buffer for zero-copy publish/consume.
- `Envelope` — wraps a delivered message with routing key, exchange, redelivery flags.
- `Properties` — AMQP headers and delivery mode.
- `ConfirmResponse` — ACK/NACK/RETURN for publisher confirms.
- `ConsumerAck`, `ConsumerAckBatch` — acknowledge/nack outcomes for consumer side.
- `Message`/`ConfirmResponse` are reused by `rmqio::EventLoop::postF` futures for async callbacks.

## Topology
- `Exchange`, `ExchangeType`, `ExchangeBinding` — exchanges and bindings.
- `Queue`, `QueueBinding`, `QueueDelete`, `QueueUnbinding` — queues, bindings, and remove operations.
- `Binding` — generic binding helper connecting exchanges to queues.
- `Topology`, `TopologyUpdate` — declarative view and incremental mutations; passed from `rmqp` to `rmqamqp`.

## Plumbing & Utilities
- `FieldValue`, `ShortString` — AMQP-friendly typed fields and bounded strings.
- `Future<T>`/`FutureUtil` — small future/promise used inside the event loop (`rmqio::EventLoop::postF`).
- `Result<T>` — value-or-error wrapper used widely for construction flows.

## Dependencies
- Pure data; no I/O. Consumed by every other library.
- Designed to be scheduler-agnostic so it can ride on any `io_context`/event loop chosen in `rmqio`.
