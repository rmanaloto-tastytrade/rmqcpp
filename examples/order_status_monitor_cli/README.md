# rmq_order_status_monitor_cli

Minimal CLI that consumes order status messages using rmqcpp, logging every delivery via Quill.

## Config inputs
- CLI flags (CLI11): host/port/vhost/username/password, exchange/direct exchange, queue, bindings, prefetch, timeouts, TLS paths.
- JSON config (Glaze): pass with `--config path.json`; keys mirror the CLI (`host`, `port`, `vhost`, `exchange`, `direct_exchange`, `queue`, `topic_bindings`, `direct_bindings`, `prefetch`, `heartbeat_ms`, `connection_timeout_ms`, `durable`, `auto_delete`, `use_tls`, `ca_cert`, `client_cert`, `client_key`, `verify_mode`).
- Environment overrides: `MQ_HOST`, `MQ_PORT`, `MQ_VHOST`, `MQ_USERNAME`, `MQ_PASSWORD`, `MQ_EXCHANGE_NAME`, `MQ_DIRECT_EXCHANGE_NAME`, `MQ_QUEUE_NAME`.
- HTTP admin (optional): `enable_http_admin`, `http_admin_user`, `http_admin_password`, `http_admin_port` (defaults to AMQP creds/15672). When enabled, the CLI can discover queues/exchanges via the management API instead of a static definitions file.

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

## RabbitMQ HTTP API crawl (dynamic topology)

When `enable_http_admin` is true, prefer live discovery via the management API. Core endpoints to crawl:

- Identity/overview: `GET /api/whoami`, `GET /api/overview` (cluster info, totals, listeners).
- Nodes: `GET /api/nodes`, then `GET /api/nodes/{name}` (and `/memory` if needed).
- Vhosts: `GET /api/vhosts`, per-vhost `GET /api/vhosts/{name}`, permissions/topic-permissions, vhost limits.
- Static topology in bulk: `GET /api/definitions` (or `/api/definitions/{vhost}`) for exchanges/queues/bindings/policies/users/etc.
- Per-vhost detail: `GET /api/exchanges/{vhost}?disable_stats=true`, `GET /api/queues/{vhost}?disable_stats=true&enable_queue_totals=true`, `GET /api/bindings/{vhost}`, `GET /api/consumers/{vhost}`. For streams, use `/api/stream/*` endpoints.
- Live client topology: `GET /api/connections` and `GET /api/channels` (paginate), or per-vhost equivalents.
- Health: `GET /api/health/checks/*` for alarms/readiness.

Implementation notes:
- Use Basic auth; guest and order-status-monitor both work on `/api/overview` in staging. If a call returns 401/403, the user is scoped down.
- Prefer per-vhost and `disable_stats=true` for bulk listings; fetch detailed stats only for a small subset.
- If HTTP admin is enabled, favor `/api/queues` (or `/api/definitions`) for live queue lists to avoid stale definitions; missing queues still log with a hint to refresh.
- RabbitMQ’s management plugin does not publish OpenAPI/Swagger; rely on the documented endpoints: https://www.rabbitmq.com/docs/http-api.

## rmqcpp internal logging (BALL)
- rmqcpp uses the BDE BALL logger internally. Without initialization you’ll see `UNINITIALIZED_LOGGER_MANAGER` prefixes.
- This CLI now bridges BALL → Quill automatically: at startup we create a `LoggerManagerScopedGuard`, register a custom BALL observer that forwards records into the configured Quill logger, and set thresholds to capture everything (TRACE/TRACE/ERROR/FATAL). All BALL records now land in the same Quill sink as application logs without the prefix.
- Adjust thresholds or sinks in the BALL init block (`examples/order_status_monitor_cli/main.cpp`) if you want to quiet categories; for deeper BALL usage, see local docs at `bloomberg/bde/groups/bal/ball/doc`.
