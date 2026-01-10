# rabbitmqadmin-ng HTTP crawl shape (reference)

Notes pulled from `rabbitmqadmin-ng` source (`src/commands.rs`, `src/main.rs`) in the upstream repo (https://github.com/rabbitmq/rabbitmqadmin-ng).

## How rabbitmqadmin-ng lists topology
- Uses the `rabbitmq_http_client` crate (blocking API).
- Overview: `client.overview()` → `GET /api/overview`.
- Vhosts: `client.list_vhosts()` → `GET /api/vhosts`.
- Exchanges (per vhost): `client.list_exchanges(vhost)` → `GET /api/exchanges/{vhost}`.
- Queues (per vhost): `client.list_queues_in(vhost)` → `GET /api/queues/{vhost}`.
- Bindings: `client.list_bindings()` → `GET /api/bindings` (global, not per-vhost in the CLI code).
- Other list operations (connections, channels, consumers, policies, users, limits, feature flags, etc.) map directly to the documented management API endpoints (see https://www.rabbitmq.com/docs/http-api-reference).

## Hierarchy to mirror
1) `/api/overview` (sanity/version/stats)
2) `/api/vhosts`
3) For each vhost:
   - `/api/exchanges/{vhost}?disable_stats=true`
   - `/api/queues/{vhost}?disable_stats=true&enable_queue_totals=true`
4) Bindings:
   - rabbitmqadmin-ng calls the *global* `/api/bindings` via `list_bindings()`.
   - Per-vhost bindings are available as `/api/bindings/{vhost}` and per exchange/queue as `/api/bindings/{vhost}/e/{exchange}/q/{queue}`.

## Pagination/flags
- The CLI code relies on the HTTP client library to handle paging; it does not set an explicit `pagination=true` flag in the CLI layer.
- Endpoints support `page`/`page_size` and `pagination=true`; the server may return 500 if flags are not accepted for a given endpoint.

## Implications for our crawler
- Keep errors visible (don’t hide 500s); bindings can fail if the server doesn’t like `pagination=true` or if permissions are insufficient.
- To align with rabbitmqadmin-ng:
  - Try `/api/bindings` (global) or `/api/bindings/{vhost}` *without* `pagination=true` if per-vhost calls return 500.
  - Continue logging per-vhost exchanges/queues with stats disabled.
- If bindings remain inaccessible, fallback options:
  - `GET /api/definitions` (full config snapshot including bindings).
  - `GET /api/bindings` without pagination flags, or per-vhost/per-exchange/queue variants.

## HTTP query flags (what they do)
- `disable_stats=true`: omit live rate/metric fields when listing objects (queues/exchanges), reducing payload size and broker work. Good for inventory crawls; use detailed per-object calls only when you need stats.
- `enable_queue_totals=true`: include queue total fields (messages, ready, unacked) even when `disable_stats` is set; gives lightweight counts without full rate metrics.

## rabbitmq_http_client capability surface (0.68.0, blocking API)
Selected list methods from `src/blocking_api`:
- Topology: `overview`, `list_vhosts`, `list_exchanges(_in)`, `list_queues(_in / _with_details)`, `list_bindings`, `list_bindings_in`, queue/exchange-specific bindings, `list_consumers(_in)`, `list_channels(_in/on)`, `list_connections(_in/user/stream)`.
- Security/config: `list_users`, `list_users_without_permissions`, permissions/topic-permissions (global/in/of user), user limits, vhost limits, policies (and operator policies) global/per-vhost, feature flags, deprecated features (all/in-use), runtime/global parameters.
- Federation/shovel: federation upstreams/links, shovels (global/in-vhost), exchange/queue federation params.
- Streams: stream publishers/consumers (global/in/on connection).
- Nodes/plugins: list nodes, cluster plugins, node plugins, node memory footprint.
- Misc: aliveness tests, health endpoints are available via other client methods; client supports TLS options and basic auth.

## Side-by-side status (our CLI vs rabbitmqadmin-ng)
- Overview: supported (matches).
- Vhosts: supported (matches).
- Exchanges: per-vhost list with `disable_stats=true` (matches).
- Queues: per-vhost list with `disable_stats=true&enable_queue_totals=true` (matches intent; rabbitmqadmin-ng does not add flags explicitly).
- Bindings: rabbitmqadmin-ng uses global `/api/bindings` via `list_bindings`; our CLI now does the same (single call, no explicit pagination flags, errors logged; latest run fetched ~1360 bindings).
- Connections/channels/consumers/policies/operator-policies/feature-flags/deprecated-features: crawled and cached.
- Users, permissions, topic-permissions, user-limits, vhost-limits, parameters, global-parameters, shovels, federation-links, nodes, node detail blobs, health checks (alarms/local-alarms/virtual-hosts/ready-to-serve), per-vhost aliveness tests, and stream endpoints (connections/publishers/consumers): crawled and cached (read-only).
- Per-object bindings fallback: if `/api/bindings` fails or returns empty, we try `/api/bindings/{vhost}` and finally `/api/definitions` for bindings only.
- Remaining read-only gaps vs rabbitmqadmin-ng: deeper node/plugin detail (structured), per-object bindings when brokers enforce pagination, and any endpoints that hard-require pagination flags.
- Definitions export: available via management API and rabbitmqadmin-ng; our CLI can read a definitions JSON if provided but prefers live crawl when enabled.

## References
- rabbitmqadmin-ng repo: https://github.com/rabbitmq/rabbitmqadmin-ng
- Management API: https://www.rabbitmq.com/docs/http-api-reference
