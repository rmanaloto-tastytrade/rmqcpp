# Monitor Example (Work in Progress)

Goal: run a single-threaded monitor on one `rmqio::AsioEventLoop` that watches RabbitMQ for topology changes and metadata, and logs every event.

## Features to Cover
- Metadata Store: if the RabbitMQ Metadata Store plugin is enabled, consume metadata messages from its exchange (per https://www.rabbitmq.com/docs/metadata-store). Also enumerate exchanges/queues/vhosts via the management API to seed state.
- Event Exchange: subscribe to `amq.rabbitmq.event` to observe declarations (https://www.rabbitmq.com/docs/event-exchange).
- Firehose Tracing: optional subscribe to `amq.rabbitmq.trace` for publish/consume traces (https://www.rabbitmq.com/docs/firehose).
- Log exchange: optional subscribe to `amq.rabbitmq.log` for broker internal events (https://www.rabbitmq.com/docs/logging#internal-events).
- Connections/channels: monitor connects/disconnects via the event exchange (connection/channel events) and management API polling (`/api/connections`, `/api/channels`).
- Dynamic binding: when new exchanges/queues appear (via event exchange), bind and start consuming their queues.
- Logging: structured logger (targeting Quill if available) to log every event and payload summary; fallback to stdout.
- Internal log exchange: subscribe to `amq.rabbitmq.log` for broker internal events if available.
- Other plugins (Shovel/Federation/Streams/MQTT/STOMP): present AMQP endpoints; consume their exchanges/queues once configured. No unified plugin event stream beyond the event exchange + management API.

## Architecture
- Single `rmqio::AsioEventLoop` (one thread) shared by all consumers.
- One connection/vhost:
  - Consumer for `amq.rabbitmq.event` (routing key pattern `#`).
  - Optional consumer for `amq.rabbitmq.trace` (firehose) gated by flag.
  - Metadata Store consumer if plugin is enabled (config flag plus runtime probe).
  - Log exchange consumer (`amq.rabbitmq.log`) gated by flag.
- Management API probe (HTTP GET to `/api/overview`, `/api/exchanges`, `/api/queues`, `/api/vhosts`) at startup to seed topology. **Note:** rmqcpp does not ship an HTTP client; integrate one (bsl sockets or external) to call the management REST API.
- Dynamic dispatcher: maintain in-memory set of known exchanges/queues; on event-exchange messages, declare/bind and start consumers on new queues.
- Logging: Quill sink if available, else `std::cout`.

## Configuration (proposed)
- `--amqp-uri` / `--username` / `--password` / `--vhost`.
- `--enable-firehose` (default off).
- `--enable-metadata-store` (default on; if broker rejects, disable).
- `--enable-event-exchange` (default on), `--enable-log-exchange` (default off).
- `--management-url` (e.g., `http://host:15672`), `--management-username` / `--management-password` for HTTP enumeration.
- `--log-level`, `--log-file`, `--use-quill` toggle.

## Implementation Notes
- Use `rmqt::Topology` to declare bindings to `amq.rabbitmq.event` (and firehose) and create `rmqa::Consumer` callbacks.
- Subscribe to `amq.rabbitmq.event` (and optionally `amq.rabbitmq.trace`, `amq.rabbitmq.log`, `amq.rabbitmq.metadata`) on the single `io_context`. Treat bind/declare failures as feature-disabled.
- For Metadata Store, follow the documented exchange/routing keys; treat it like a normal consumer.
- Keep all callbacks on the single `io_context`; avoid extra threads.
- Consider backpressure: cap prefetch on event/firehose consumers to avoid overwhelming the loop; drop/skip large payload logging in summary mode.
- Security: TLS via `rmqt::SecureEndpoint` if needed; management API TLS handled by chosen HTTP client.
- Poll the management API for current exchanges/queues/vhosts/connections/channels and `running_plugins` to see what’s enabled; merge with the event stream to discover new objects. React to event-exchange messages to bind to new queues as they appear.
- To observe payloads without stealing them from application consumers, rely on firehose or arrange a topology tee (app exchange routes to both the app queue and a monitor queue). Avoid consuming directly from application queues.
- Keep the monitor single-threaded on one `io_context` to minimize locks; prefer string views/spans over owned strings in hot paths to reduce allocations while logging/dispatching.

## References
- Event exchange docs: https://www.rabbitmq.com/docs/event-exchange
- Firehose tracing: https://www.rabbitmq.com/docs/firehose
- Metadata Store: https://www.rabbitmq.com/docs/metadata-store
- Internal events logging: https://www.rabbitmq.com/docs/logging#internal-events
- RabbitMQ server source: https://github.com/rabbitmq/rabbitmq-server
- RabbitMQ website/docs: https://github.com/rabbitmq/rabbitmq-website, https://www.rabbitmq.com/docs
- RabbitMQ tutorials: https://github.com/rabbitmq/rabbitmq-tutorials, https://www.rabbitmq.com/tutorials
- Boost.Asio: https://github.com/boostorg/asio (docs: https://www.boost.org/doc/libs/latest/doc/html/boost_asio.html)
