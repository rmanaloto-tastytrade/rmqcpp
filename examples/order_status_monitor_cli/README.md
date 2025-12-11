# rmq_order_status_monitor_cli

Minimal CLI that consumes order status messages using rmqcpp, logging every delivery via Quill.

## Config inputs
- CLI flags (CLI11): host/port/vhost/username/password, exchange/direct exchange, queue, bindings, prefetch, timeouts, TLS paths.
- JSON config (Glaze): pass with `--config path.json`; keys mirror the CLI (`host`, `port`, `vhost`, `exchange`, `direct_exchange`, `queue`, `topic_bindings`, `direct_bindings`, `prefetch`, `heartbeat_ms`, `connection_timeout_ms`, `durable`, `auto_delete`, `use_tls`, `ca_cert`, `client_cert`, `client_key`, `verify_mode`).
- Environment overrides: `MQ_HOST`, `MQ_PORT`, `MQ_VHOST`, `MQ_USERNAME`, `MQ_PASSWORD`, `MQ_EXCHANGE_NAME`, `MQ_DIRECT_EXCHANGE_NAME`, `MQ_QUEUE_NAME`.
- HTTP admin (optional): `enable_http_admin`, `http_admin_user`, `http_admin_password`, `http_admin_port` (defaults to AMQP creds/15672). When enabled, the CLI can discover queues/exchanges via the management API instead of a static definitions file.
- Queue allow-list: `queue_whitelist` (JSON/CLI `--queue-whitelist`) restricts which queues from HTTP discovery are included; others are logged as filtered.
- Logging/health: `ball_min_severity` (json/env `BALL_MIN_SEVERITY`, cli `--ball-min-severity`) sets the minimum BALL level forwarded to Quill (`trace|debug|info|warn|error|fatal`, default `trace`). `thread_pool_queue_depth` tunes the single-thread callback queue (default 200000). A periodic health log reports messages and enqueue failures every 10s.
- BALL noisy categories: override `ball_noisy_prefixes` (default: `RMQAMQP.CHANNEL`, `RMQAMQP.CONNECTION`, `RMQIO.ASIOEVENTLOOP`, `RMQIO.EVENTLOOP`) and `ball_noisy_min_severity` (default INFO/WARN) to drop chatty categories below a threshold while still forwarding everything else to Quill.
- OpenTelemetry (requires opentelemetry-cpp via vcpkg): `enable_otel`, `enable_otel_traces`, `enable_otel_metrics`, `otel_protocol` (`grpc`|`http`), `otel_endpoint` (grpc host:port or http URL), `otel_service_name`, `otel_environment`. When enabled, spans/metrics are exported via OTLP.
- BALL→Quill logging: use `ball_min_severity` (json/env `BALL_MIN_SEVERITY`, cli `--ball-min-severity`) to set the minimum BALL level forwarded to Quill (`trace|debug|info|warn|error|fatal`, default `trace`).
- Connection resilience: `heartbeat_ms` (default 60000) for AMQP heartbeats, `connection_timeout_ms` (default 10000), optional `connection_error_threshold_ms` (when >0, triggers the error callback if no connection is established within that window), `shuffle_connection_endpoints` to randomize resolver results, and `infinite_immediate_retry` to enable rmqcpp’s IIR tunable (retry forever without sleeping). Noisy BALL categories can be filtered with `ball_noisy_prefixes` and `ball_noisy_min_severity` (default prefixes: RMQAMQP.CHANNEL/CONNECTION, RMQIO.ASIOEVENTLOOP/EVENTLOOP; default min severity INFO/WARN).

Default topic binding: `accounts.*.orders.*.*` if none provided.

Static example (Nomad env) `config.static.example.json` (HTTP admin off, password redacted):
```json
{
  "host": "staging-rabbit-cluster.tastyworks.com",
  "port": 5672,
  "vhost": "/",
  "username": "order-status-monitor",
  "password": "REDACTED",
  "exchange": "services.api",
  "direct_exchange": "oel.direct",
  "queues": ["order_status_monitor"],
  "topic_bindings": [
    "ch2_citadel_equities_b_accounts_orders_cancel_requested",
    "ch2_zero_hash_clob_a_accounts_orders_cancel_requested",
    "ch2_test_futures_a_accounts_orders_received",
    "ch2_jane_street_cryptocurrency_a_accounts_orders_cancel_requested",
    "ch2_test_futures_b_accounts_orders_cancel_requested",
    "ch2_jane_street_equities_a_accounts_orders_received",
    "ch2_cme_cert_a_accounts_orders_cancel_requested",
    "ch2_susquehanna_equities_b_accounts_orders_cancel_requested",
    "ch2_test_a_accounts_orders_received",
    "ch2_virtu_equities_b_accounts_orders_received",
    "ch2_jane_street_options_a_accounts_orders_cancel_requested",
    "ch2_citadel_options_a_accounts_orders_received",
    "ch2_susquehanna_equities_a_accounts_orders_received",
    "ch2_smalls_b_accounts_orders_received",
    "ch2_citadel_equities_a_accounts_orders_received",
    "ch2_susquehanna_equities_a_accounts_orders_cancel_requested",
    "ch2_cme_cgw_a_accounts_orders_cancel_requested",
    "ch2_morgan_stanley_options_a_accounts_orders_received",
    "ch2_test_c_accounts_orders_received",
    "ch2_hudson_river_trading_a_accounts_orders_cancel_requested",
    "ch2_test_b_accounts_orders_received",
    "ch2_wolverine_options_a_accounts_orders_cancel_requested",
    "ch2_wolverine_equities_a_accounts_orders_received",
    "ch2_dash_equities_b_accounts_orders_cancel_requested",
    "ch2_jane_street_equities_b_accounts_orders_cancel_requested",
    "ch2_cme_cgw_binary_a_accounts_orders_received",
    "ch2_cme_cgw_binary_b_accounts_orders_cancel_requested",
    "ch2_cme_cgw_c_accounts_orders_cancel_requested",
    "ch2_cfe_b_accounts_orders_cancel_requested",
    "ch2_susquehanna_options_b_accounts_orders_cancel_requested",
    "ch2_citadel_cryptocurrency_a_accounts_orders_received",
    "ch2_jane_street_options_b_accounts_orders_received",
    "ch2_wolverine_equities_a_accounts_orders_cancel_requested",
    "ch2_zero_hash_clob_a_accounts_orders_received",
    "ch2_jane_street_equities_a_accounts_orders_cancel_requested",
    "ch2_hudson_river_trading_a_accounts_orders_received",
    "ch2_test_a_accounts_orders_cancel_requested",
    "ch2_cme_cgw_binary_a_accounts_orders_cancel_requested",
    "ch2_citadel_equities_a_accounts_orders_cancel_requested",
    "ch2_virtu_equities_a_accounts_orders_cancel_requested",
    "ch2_citadel_options_a_accounts_orders_cancel_requested",
    "ch2_cfe_a_accounts_orders_received",
    "ch2_test_futures_a_accounts_orders_cancel_requested",
    "ch2_cme_cgw_b_accounts_orders_received",
    "ch2_zero_hash_clob_b_accounts_orders_received",
    "ch2_test_futures_c_accounts_orders_cancel_requested",
    "ch2_dash_options_a_accounts_orders_received",
    "ch2_cme_cgw_b_accounts_orders_cancel_requested",
    "ch2_test_futures_b_accounts_orders_received",
    "ch2_cfe_a_accounts_orders_cancel_requested",
    "ch2_morgan_stanley_options_a_accounts_orders_cancel_requested",
    "ch2_test_d_accounts_orders_cancel_requested",
    "ch2_wolverine_options_a_accounts_orders_received"
  ],
  "direct_bindings": [],
  "enable_http_admin": false
}
```

## Behavior
- Single-threaded `rmqio::AsioEventLoop` (epoll on Linux, kqueue on macOS).
- Declares topic/direct exchanges and queue, binds configured routing keys.
- Creates a consumer with explicit acks and configurable prefetch; logs every delivery (`exchange`, `routing key`, `body size`, `delivery tag`) via Quill.
- Graceful shutdown on SIGINT/SIGTERM; optional `--run-seconds` to stop after a duration for connectivity tests.

## Build
```bash
cmake --build <build-dir> --target rmq_order_status_monitor_cli
```

### Compare CLI vs live /api/overview
If Python 3 is available, CMake adds a helper target to diff the latest CLI-emitted `http_overview-*.json` against a fresh curl of `/api/overview`:
```bash
cmake --build <build-dir> --target order_status_monitor_cli_diff
```
It reads `config.local.json` for HTTP admin host/user/password (or override with CLI flags inside `tools/compare_overview.py`) and writes `curl_overview-*.json` plus `overview_diff-*.txt` into `<build-dir>/examples/order_status_monitor_cli/logs/`.

### Env/JSON/CLI precedence
Defaults → JSON (`--config`) → environment overrides (e.g., `MQ_*`) → CLI flags. In Nomad, Vault/Consul templates populate `MQ_*` and binding env vars; local JSON/CLI can override for testing.

### Nomad-style static run
Use a static config (HTTP admin off) with env-provided queue/bindings. For example:
```
./rmq_order_status_monitor_cli --config ./examples/order_status_monitor_cli/config.local.static.json
```
Ensure `MQ_PASSWORD` (and other secrets) are set in the environment or config before running. The sample static config mirrors the staging Nomad env.

### Reconnect sanity check
To validate reconnect/rebind behavior, you can force-close the AMQP connection via the management API and watch the logs:
1. Start the CLI with a short run: `--run-seconds 60` and your config.
2. In another shell, find the connection name via `/api/connections` and `DELETE /api/connections/{name}` (Basic auth).
3. The CLI should log a disconnect, retry according to the backoff settings, reconnect, and rebind the consumer.

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
- Chatty RMQAMQP channel/connection categories are suppressed below WARN in the bridge; raise `ball_min_severity` or adjust the observer in `main.cpp` if you want more noise.
- Adjust thresholds (`ball_min_severity`) or sinks in the BALL init block (`examples/order_status_monitor_cli/main.cpp`) if you want to quiet categories; for deeper BALL usage, see local docs at `bloomberg/bde/groups/bal/ball/doc`.
