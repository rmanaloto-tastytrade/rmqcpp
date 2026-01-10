# Repository Guidelines

## Project Structure & Module Organization
- Core libraries live under `src/rmq`: `rmqp` (public interfaces), `rmqa` (implementations), `rmqt` (data types), `rmqamqp`/`rmqamqpt` (protocol), and `rmqio` (I/O abstractions).
- Test utilities are in `src/rmqtestmocks` and `src/tests` (`*.t.cpp` unit tests, `*.m.cpp` test mains). Python-based integration suites and fixtures reside in `src/tests/integration`.
- Examples are under `examples/` (e.g., `examples/helloworld`, `examples/topology`, `examples/rmqperftest`), useful for smoke-testing changes.
- Build/CI configuration is driven by `CMakeLists.txt`, `CMakePresets.json`, and the top-level `Makefile`. Docker tooling sits in `dockerfiles/`.

## Build, Test, and Development Commands
- Prereqs: set `VCPKG_ROOT` and choose a `CMAKE_PRESET` from `CMakePresets.json` when building locally; zstd is on by default (disable with `-DENABLE_COMPRESSION=OFF`).
- `make init` — configure CMake with the selected preset and vcpkg toolchain.
- `make build` — incremental library build (no tests).
- `make unit` or plain `make` — build and run the unit suites.
- Docker path (no local vcpkg needed): `make docker-setup`, then `make docker-build` or `make docker-unit`; use `make docker-shell` for an interactive environment.

## Coding Style & Naming Conventions
- C++17 codebase formatted with `.clang-format` (4-space indent, 80-col, Stroustrup braces, no tabs). Run `clang-format` before sending patches.
- Namespaces follow `BloombergLP::rmq*`; prefer descriptive type names and explicit ownership semantics (`bsl::shared_ptr`, `bsl::unique_ptr`).
- File naming mirrors package (`rmqp_foo.cpp`, `rmqio_bar.h`); keep one major class per file where practical.
- Preserve license headers at the top of source files.

## Testing Guidelines
- Primary framework: GoogleTest/GoogleMock (`#include <gtest/gtest.h>`, `<gmock/gmock.h>`). Unit test files end with `.t.cpp`; test mains end with `.m.cpp`.
- Add focused cases near the code under test; use existing mocks in `src/rmqtestmocks` and helpers in `src/tests/rmqtestutil`.
- Integration tests (Python + Docker) live in `src/tests/integration`; run them inside the provided Docker environment when touching network/protocol surfaces.

## Commit & Pull Request Guidelines
- Use imperative, concise commit subjects similar to existing history (e.g., “Add support for message compression”) and include `-s/--signoff` for DCO compliance.
- PRs should link issues (`#<issue number>`), describe behavior changes, note test coverage (`make unit`, docker tests if applicable), and include reproduction steps for bug fixes.
- Add screenshots only when UI-affecting (rare here); otherwise favor logs or command outputs demonstrating the fix.

## Security & Configuration Tips
- Follow `SECURITY.md` for vulnerability reporting; avoid committing secrets or broker credentials.
- When running brokers locally for integration tests, isolate via Docker Compose (`dockerfiles/docker-compose.yaml`) and clean up containers/volumes afterward.
