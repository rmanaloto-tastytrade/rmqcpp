# Modern C++26 RabbitMQ Admin Client (rabbitmqadmin successor)

Goal: build a C++26, libc++/clang-22-based replacement for the Python `rabbitmqadmin` tool from the RabbitMQ management plugin, with clear separation of concerns and reusable libraries.

Upstream reference: `rabbitmq/rabbitmq-management/priv/www/cli/rabbitmqadmin` (management HTTP API client).

## Requirements
- C++26, clang-22, libc++ (current toolchain).
- Single-threaded Asio `io_context` (reuse rmqcpp/rmqio where it helps; otherwise standalone Beast HTTP client).
- HTTPS with TLS, basic auth, optional client certs; configurable management endpoint (host, port, vhost base path).
- Zero global state; allocator-aware where feasible; avoid heap churn in hot paths (pmr arenas); avoid locks.
- Fast startup and low latency for list/show/publish commands.

## Proposed Module Breakdown
- `admin_core`: command registry, request/response model (strong types for queues, exchanges, bindings, connections, channels, vhosts, users/permissions).
- `admin_http`: Boost.Beast + OpenSSL client wrapper with retry/backoff, JSON codec (Boost.JSON or another header-only JSON).
- `admin_cli`: CLI11-based parser, config file/env var overlay, help/usage generation.
- `admin_exec`: glue that maps CLI verbs to HTTP calls, handles pagination and formatting (table/JSON/yaml).
- `admin_render`: output formatters (plain, JSON pass-through), column selection, wide/narrow modes.

## Command Parity (initial scope)
- Read-only: list/show vhosts, queues, exchanges, bindings, connections, channels, consumers, policies, parameters, nodes, overview, aliveness, health checks, permissions.
- Mutating: declare/delete queue/exchange/binding, publish (basic.publish), purge queue, set/delete policy, set/delete parameter, create/delete vhost, create/delete user, set permissions (optional, later).
- Compatible query args: pagination, name filters, columns selection where management API supports it.

## Architecture Notes
- HTTP: Beast async client on a dedicated `io_context`; keep single-threaded; optional connection pooling for multiple requests in a batch; TLS via OpenSSL (cert/key/CA, insecure toggle for dev).
- JSON: Prefer Boost.JSON for zero-copy string views; decode into lightweight structs; avoid dynamic allocations where possible (use pmr).
- CLI: CLI11; support env vars (`RABBITMQ_URL`, `RABBITMQ_USER`, `RABBITMQ_PASSWORD`, `RABBITMQ_TLS_*`), config file (`~/.config/rmqadmin/config.yaml` in future).
- Logging: quill (console sink) with structured fields; quiet/verbose flags.
- Output: default table (width-aware), `--format json` to return raw API payload, `--format pretty-json` optional.
- Error handling: propagate HTTP status and body; map well-known errors (404 exchange not found, 401/403 auth, 400 bad request).
- Testing: unit tests for command mapping and JSON decoding; integration tests (Docker RabbitMQ with management plugin) for a core subset (list vhosts, list queues, declare/publish/purge).

## Performance/Quality Practices
- Use pmr arenas for request/response assembly; avoid copies (string_view where safe).
- No locks; single-threaded event loop.
- Minimize allocations in CLI hot path; preallocate buffers for HTTP requests.
- Reuse connections for multi-call operations (e.g., list + fetch details).

## Open Questions / TODO
- Choose JSON library (Boost.JSON default to avoid extra deps).
- Auth variants: username/password only in v1; add OAuth2/token later?
- Config file format (YAML/JSON) and priority vs env/CLI.
- Do we interop with rmqcpp types for any AMQP actions (e.g., optional publish via AMQP instead of HTTP)?

## Next Steps
1) Scaffold `admin_core`, `admin_http`, `admin_cli`, `admin_exec` libraries and an `rmqadmin` binary target.
2) Implement read-only commands: list/show vhosts, queues, exchanges, bindings, connections, overview.
3) Add declare/delete queue/exchange/binding and publish.
4) Add table/json output formatting and basic tests.
5) Add integration harness against Dockerized RabbitMQ with management plugin enabled.
