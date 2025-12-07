# rmq_order_status_monitor_cli

Minimal CLI that consumes order status messages using rmqcpp, logging every delivery via Quill.

## Config inputs
- CLI flags (CLI11): host/port/vhost/username/password, exchange/direct exchange, queue, bindings, prefetch, timeouts, TLS paths.
- JSON config (Glaze): pass with `--config path.json`; keys mirror the CLI (`host`, `port`, `vhost`, `exchange`, `direct_exchange`, `queue`, `topic_bindings`, `direct_bindings`, `prefetch`, `heartbeat_ms`, `connection_timeout_ms`, `durable`, `auto_delete`, `use_tls`, `ca_cert`, `client_cert`, `client_key`, `verify_mode`).
- Environment overrides: `MQ_HOST`, `MQ_PORT`, `MQ_VHOST`, `MQ_USERNAME`, `MQ_PASSWORD`, `MQ_EXCHANGE_NAME`, `MQ_DIRECT_EXCHANGE_NAME`, `MQ_QUEUE_NAME`.

Default topic binding: `accounts.*.orders.*.*` if none provided.

## Behavior
- Single-threaded `rmqio::AsioEventLoop` (epoll on Linux, kqueue on macOS).
- Declares topic/direct exchanges and queue, binds configured routing keys.
- Creates a consumer with explicit acks and configurable prefetch; logs every delivery (`exchange`, `routing key`, `body size`, `delivery tag`) via Quill.
- Graceful shutdown on SIGINT/SIGTERM; optional `--run-seconds` to stop after a duration for connectivity tests.

## Build
```bash
cmake --build <build-dir> --target rmq_order_status_monitor_cli
```
