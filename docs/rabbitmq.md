# RabbitMQ Primer

RabbitMQ is a message broker implementing AMQP 0-9-1 (and AMQP 1.0 via the `rabbitmq_amqp1_0` plugin). It provides durable queues, exchanges, routing, confirms/acks, and clustering/HA features. Official docs and source are maintained by the RabbitMQ team:

- Reference docs: https://www.rabbitmq.com/docs and https://www.rabbitmq.com/tutorials (source: [`rabbitmq/rabbitmq-website`](https://github.com/rabbitmq/rabbitmq-website), [`rabbitmq/rabbitmq-tutorials`](https://github.com/rabbitmq/rabbitmq-tutorials))
- Server source: [`rabbitmq/rabbitmq-server`](https://github.com/rabbitmq/rabbitmq-server)
- General tutorials: https://www.rabbitmq.com/tutorials

## Protocols
- AMQP 0-9-1: fully supported by rmqcpp (maps to `rmqamqpt` types and `rmqamqp` channel state machines).
- AMQP 1.0: not implemented by rmqcpp; requires the RabbitMQ plugin and a different client.
- Other protocols (MQTT, STOMP, WebSockets) are not implemented by rmqcpp; RabbitMQ supports them via plugins for other clients.

## RabbitMQ Features and rmqcpp Coverage
- Exchanges/Queues/Bindings: supported via `rmqt::Topology` and `rmqa`/`rmqamqp` declarative flows.
- Publisher Confirms & Consumer Acks: enabled by default in rmqcpp (`rmqa::Producer`, `rmqa::Consumer`); see confirms/acks docs on RabbitMQ: https://www.rabbitmq.com/confirms.html, https://www.rabbitmq.com/confirms.html#consumer-acks
- Mandatory flag: default true in rmqcpp to avoid silent drops; aligns with routing guarantees.
- TLS: supported via `rmqt::SecureEndpoint`/`rmqt::SecurityParameters`.
- Heartbeats: negotiated and enforced; see [`docs/heartbeats.md`](./heartbeats.md).
- Clustering/HA/Mirroring/Quorum queues: server-side features; rmqcpp works with these via standard AMQP semantics. Configuration is handled on the broker (see https://www.rabbitmq.com/docs), not in the client API.
- Plugins (Shovel/Federation/Streams/MQTT/STOMP): not directly surfaced; rmqcpp speaks AMQP 0-9-1 and can interoperate if the broker presents AMQP endpoints for the plugin.

## Event Exchange, Firehose, Metadata Store, Internal Logging
These features expose AMQP 0-9-1 exchanges/queues; rmqcpp can be used to implement clients for all of them via standard declares/binds/publish/consume.
- Event Exchange: https://www.rabbitmq.com/docs/event-exchange — use `rmqt::Topology` to declare/bind queues to the event exchange, publish/consume with `rmqa::Producer`/`rmqa::Consumer`.
- Firehose Tracing: https://www.rabbitmq.com/docs/firehose — bind a queue to `amq.rabbitmq.trace` and consume with `rmqa::Consumer` once tracing is enabled on the broker.
- Metadata Store: https://www.rabbitmq.com/docs/metadata-store — publish/consume metadata messages with `rmqa::Producer`/`rmqa::Consumer` using the documented routing keys/exchange.
- Internal Events Logging: https://www.rabbitmq.com/docs/logging#internal-events — subscribe to the broker log/event exchanges with `rmqa::Consumer` to receive internal events.

## Useful RabbitMQ Repos/Docs
- `rabbitmq/rabbitmq-website` — source for https://www.rabbitmq.com/docs and tutorials.
- `rabbitmq/rabbitmq-tutorials` — code samples in various languages (AMQP basics, RPC, pub-sub).
- `rabbitmq/rabbitmq-server` — broker source including plugins.

## Building with rmqcpp
All of the above AMQP 0-9-1-based features can be implemented in C++ using rmqcpp by declaring the appropriate exchanges/queues/bindings (see `rmqt::Topology`), publishing with `rmqa::Producer`, and consuming with `rmqa::Consumer`. For plugin-specific exchanges (event exchange, firehose, metadata store, internal events), ensure the plugin/feature is enabled on the broker, then bind/consume using standard AMQP operations.
