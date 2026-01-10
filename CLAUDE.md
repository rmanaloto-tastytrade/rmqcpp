# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

`rmqcpp` is a C++ library for RabbitMQ that provides a testable, data-safety-focused, and async-capable API. It's built with reliability and message safety as primary concerns, featuring automatic heartbeats, reconnection logic, and publisher confirmations by default.

## Build System

### Prerequisites
- **vcpkg**: Primary dependency manager. Set `VCPKG_ROOT` environment variable.
- **CMake 3.25+**: Uses CMake presets for configuration.
- **C++17**: Required standard.
- **Ninja**: Recommended build generator.

### Build Commands

**Standard Build Flow:**
```bash
# Initialize build with preset (creates build directory)
make init CMAKE_PRESET=macos-arm64-vcpkg

# Incremental build
make build

# Run unit tests
make unit

# Full build + test
make full-test

# Clean build directory
make clean
```

**Available CMake Presets** (set via `CMAKE_PRESET` env var):
- `macos-arm64-vcpkg` (default)
- `macos-x64-vcpkg`
- `linux-x64-vcpkg`

**Docker Development:**
```bash
# Setup docker environment (required first step)
make docker-setup

# Build in container
make docker-build

# Test in container
make docker-test

# Interactive shell in build container
make docker-shell
```

**Integration Tests:**
```bash
# Requires running RabbitMQ instance
make integration
# Uses environment: RMQ_USER, RMQ_PWD, RMQ_PORT (5672), RMQ_TLS_PORT (5671)
```

**Running Individual Tests:**
```bash
# Run specific test executable
./build/src/tests/rmqa/rmqa_topology.t

# Run tests through ctest with filters
ctest --test-dir ./build --output-on-failure -R rmqa_topology
```

### Build Configuration

**Compression Support:**
- Enabled by default (requires `zstd` library)
- Disable with: `cmake -DENABLE_COMPRESSION=OFF`

**Important Build Variables:**
- `CMAKE_CXX_STANDARD=17` (required)
- `CMAKE_INSTALL_LIBDIR=lib64`
- `BDE_BUILD_TARGET_SAFE=true`
- `CMAKE_BUILD_TYPE=Debug`

## Architecture

### Package Hierarchy

The library is organized into layered packages with clear dependencies:

```
rmqa (Application Interface)
  ↓
rmqamqp (AMQP Protocol Layer)
  ↓
rmqio (I/O Abstraction)
  ↓
rmqamqpt (AMQP Types)
```

**Public Packages** (applications use these):
- **rmqa**: Main concrete implementation (RabbitContext, Producer, Consumer, VHost)
- **rmqp**: Protocol interfaces for testing/mocking
- **rmqt**: Data types (Message, Topology, Credentials, Endpoint)
- **rmqtestmocks**: gmock objects for application testing

**Internal Packages** (not directly used by applications):
- **rmqamqp**: AMQP protocol state machines, channels, connections
- **rmqamqpt**: Low-level AMQP data types from 0-9-1 spec
- **rmqio**: Socket connections, boost::asio implementation

### Key Architectural Patterns

**Event Loop Threading:**
- One main event loop per `RabbitContext` handles all I/O
- Event loop components (rmqamqp, rmqio, rmqamqpt) must ONLY run on event loop thread
- Client code (rmqa) must never block event loop
- Callbacks to client code are dispatched to separate threadpool
- rmqa components must never destruct event loop components directly

**Connection Management:**
- Separate connections for publishing and consuming (prevents backpressure issues)
- Automatic reconnection on network failures
- Topology redeclaration on reconnect
- Heartbeat management is automatic and transparent

**Message Safety:**
- Publisher confirmations required by default
- Consumer acknowledgments required (no autoack)
- Durable queues and persistent delivery mode by default
- Mandatory flag defaulted to true (detects routing failures)

## Code Conventions

### File Naming
- Headers: `packagename_componentname.h`
- Implementation: `packagename_componentname.cpp`
- Test files: `packagename_componentname.t.cpp` (suffix `.t.cpp`)
- Main programs: `*.m.cpp`

### Namespace
- All code in `BloombergLP` namespace
- Package namespaces: `rmqa`, `rmqt`, `rmqp`, `rmqamqp`, `rmqio`, `rmqamqpt`

### Code Style
- **Formatting**: Uses `.clang-format` (clang-format-20)
- **Column Limit**: 80 characters
- **Indentation**: 4 spaces (never tabs)
- **Brace Style**: Stroustrup
- **Pointer Alignment**: Left (`Type* ptr`)
- Check format: `clang-format --style=file --dry-run --Werror src/**/*.{h,cpp}`

### Dependencies
- Uses Bloomberg BDE libraries (bsl, bdlb, ball, etc.)
- boost::asio for async I/O
- Google Test/Mock for testing
- OpenSSL for TLS
- zstd for optional compression

## Testing

### Test Structure
- Unit tests in `src/tests/` mirroring source structure
- Test files use `.t.cpp` suffix
- Integration tests in `src/tests/integration/`
- Test utilities in `src/tests/rmqtestutil/`
- Mock objects in `src/rmqtestmocks/`

### Running Tests
```bash
# All unit tests
make unit

# All tests including integration
make full-test

# Single test file
./build/src/tests/rmqa/rmqa_topology.t

# CTest with pattern
ctest --test-dir ./build -R "rmqa.*" --output-on-failure
```

## Contributing

### Commit Requirements
- All commits must include `Signed-Off-By` line
- Add with: `git commit -s` or `git commit --signoff`
- Use real name (no pseudonyms)

### Pull Request Process
1. Create Issue with 'Feature Request' template
2. Submit PR linking to issue with "#<issue number>"
3. Ensure formatting checks pass
4. All tests must pass

### Pre-commit Checks
- **Formatting**: clang-format-20 on `src/` and `examples/`
- **Linting**: ShellCheck on shell scripts
- **Tests**: Docker build and test workflow

## Important Constraints

### Thread Safety Rules
- Event loop components are NOT thread-safe
- rmqa components must use proper synchronization when calling from multiple threads
- Never call event loop components directly from client threads
- All client callbacks are dispatched to separate threadpool

### Message Handling
- MessageGuard must be used for consumer messages
- Always call `ack()` or `nack()` on MessageGuard
- Auto-nack on MessageGuard destruction if not explicitly handled
- Producer `send()` blocks if `maxOutstandingConfirms` reached
- Always wait for confirmations before shutdown: `producer->waitForConfirms()`

### Topology Management
- Topology must be declared before creating producers/consumers
- Topology is automatically redeclared on reconnection
- Use `updateTopology()` to modify topology for existing producers/consumers
- Auto-generated queues: use `topology.addQueue()` without name parameter

## Common Patterns

### Basic Producer Setup
```cpp
rmqa::RabbitContext rabbit;
rmqa::Topology topology;
auto exchange = topology.addExchange("my-exchange");
auto vhost = rabbit.createVHostConnection("conn-name", endpoint, credentials);
rmqt::Result<rmqa::Producer> result = vhost->createProducer(topology, exchange, maxOutstandingConfirms);
// Always check result before use
if (!result) { /* handle error */ }
```

### Basic Consumer Setup
```cpp
auto queue = topology.addQueue("my-queue");
topology.bind(exchange, queue, "routing-key");
rmqt::Result<rmqa::Consumer> result = vhost->createConsumer(
    topology, queue, consumerCallback, "label", prefetchCount);
```

### Clean Shutdown
```cpp
producer->waitForConfirms(timeout);  // Wait for outstanding confirms
consumer->cancelAndDrain();          // Cancel consumer and drain messages
// RabbitContext destructor handles cleanup
```

## Documentation

- API Documentation: https://bloomberg.github.io/rmqcpp/index.html
- Generated with Doxygen from source comments
- Thread safety rules: `docs/thread-safety-rules.md`
- Event loop details: `docs/eventloop.md`
- Heartbeats: `docs/heartbeats.md`
