# Docker Tooling

This repo includes Dockerfiles and Compose assets under `dockerfiles/` to build, test, or proxy rmqcpp without installing local dependencies.

- [`dockerfiles/dev.Dockerfile`](../dockerfiles/dev.Dockerfile) — development image with toolchain, vcpkg, and dependencies to build/test rmqcpp (used by `make docker-setup`, `make docker-build`, `make docker-unit`).
- [`dockerfiles/amqpprox.Dockerfile`](../dockerfiles/amqpprox.Dockerfile) — builds amqpprox sidecar for proxying/observability in front of RabbitMQ during tests.
- [`dockerfiles/docker-compose.yaml`](../dockerfiles/docker-compose.yaml) — brings up RabbitMQ and supporting services for integration tests; referenced by `src/tests/integration/README.md`.
- [`dockerfiles/start_proxy.sh`](../dockerfiles/start_proxy.sh) — helper to start amqpprox from the Compose environment.

Usage:
- Run `make docker-setup` to build the dev image and vcpkg cache.
- `make docker-build` / `make docker-unit` to build and run tests inside the container.
- `make docker-shell` for an interactive shell in the dev image.
- For integration tests, use the Compose file above to start the broker/proxy stack.
