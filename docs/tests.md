# Tests Overview

This catalog lists the test suites under `src/tests/` and links to per-suite markdown that describes what each test does, required configuration, and key link libraries.

- Unit: [`src/tests/rmqa/README.md`](../src/tests/rmqa/README.md), [`src/tests/rmqamqp/README.md`](../src/tests/rmqamqp/README.md), [`src/tests/rmqamqpt/README.md`](../src/tests/rmqamqpt/README.md), [`src/tests/rmqio/README.md`](../src/tests/rmqio/README.md), [`src/tests/rmqt/README.md`](../src/tests/rmqt/README.md), [`src/tests/rmqtestmocks/README.md`](../src/tests/rmqtestmocks/README.md), [`src/tests/rmqtestutil/README.md`](../src/tests/rmqtestutil/README.md).
- Integration (Python + Docker): [`src/tests/integration/README.md`](../src/tests/integration/README.md) — covers producer/consumer/topology/exit/perf scenarios and relies on Docker Compose plus broker credentials from the fixtures. Targets link `rmqintegration` and run against live brokers; Compose file at [`src/tests/integration/docker-compose.yml`](../src/tests/integration/docker-compose.yml).
- Performance smoke scripts: [`src/tests/performance/README.md`](../src/tests/performance/README.md) — shell wrappers for producer/consumer throughput checks.

## Why target_link_libraries differ
- Data-only layers ([`rmqt`](../src/rmq/rmqt/README.md), mocks) link fewer dependencies for fast, deterministic tests.
- Protocol and IO layers ([`rmqamqp`](../src/rmq/rmqamqp/README.md), [`rmqamqpt`](../src/rmq/rmqamqpt/README.md), [`rmqio`](../src/rmq/rmqio/README.md), [`rmqa`](../src/rmq/rmqa/README.md)) pull in transitive link libraries so framing, timers, and retries are exercised end-to-end.
- Integration/performance suites link `rmqintegration`/`rmq` because they build full clients that talk to a broker; Python harnesses in `src/tests/integration` manage credentials/endpoints.

## Authentication & Configuration
- Integration tests read endpoints/credentials from `src/tests/integration/fixtures.py` and `rmqintegration_testparameters.*`; Docker Compose (`src/tests/integration/docker-compose.yml`) spins up the broker.
- Unit tests that touch TLS endpoints use in-repo fixtures from `rmqt_secureendpoint` cases and do not require external secrets.

## Event Loop Considerations
- IO-heavy suites (`rmqio`, `rmqa`, `rmqamqp`) run on the single-threaded [`rmqio::AsioEventLoop`](../src/rmq/rmqio/rmqio_asioeventloop.h), so they naturally demonstrate how RabbitMQ traffic shares a [`boost::asio::io_context`](https://www.boost.org/doc/libs/latest/doc/html/boost_asio.html) with other descriptors (including io_uring-enabled Asio builds).
