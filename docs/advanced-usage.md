# Advanced Usage: Low Latency & High Throughput

See also: [`docs/libraries.md`](./libraries.md) for package links and the per-library READMEs under `src/rmq/` (`rmqio`/`rmqa`/`rmqp`/`rmqt`/`rmqamqp`/`rmqamqpt`) for class-level details.

Guidance for squeezing the most out of `rmqcpp`, with emphasis on single-threaded, allocation-aware, and lock-minimized setups that share a [`Boost.Asio`](https://github.com/boostorg/asio) [`io_context`](https://www.boost.org/doc/libs/latest/doc/html/boost_asio.html) (including io_uring builds).

## macOS build note (BDE)
- The BDE CMake exports inject `-lrt`/`-lstdc++` into `bslTargets.cmake`, which does not exist on macOS. Our vcpkg BDE overlay strips those flags after install; keep the overlay enabled (via `configuration.overlay-ports: ["vcpkg-overlays"]` in `vcpkg.json`) when building on macOS.

## Single-Threaded Event Loop
- Keep the default `rmqio::AsioEventLoop` single-threaded; run your other Asio work (epoll/kqueue/io_uring descriptors, timers, sockets, REST clients) on the same `io_context` via `AsioEventLoop::context()`.
- Pin the event loop thread to a dedicated core if you want predictable latency; avoid binding other CPU-heavy work to that core.
- The library owns the event loop thread and a callback thread pool by default. To stay single-threaded end-to-end, inject a one-thread `bdlmt::ThreadPool` via `RabbitContextOptions::setThreadpool` or via `rmqt::ConsumerConfig::setThreadpool` per-consumer. Alternatively provide a custom `rmqio::EventLoop` (or `AsioEventLoop`) at `RabbitContextImpl` construction if you need to own the `io_context` lifecycle yourself.
- Prefer `post/dispatch` on the event loop instead of spawning threads. Avoid long blocking work inside callbacks; hand off to another queue if needed.

## Avoiding Runtime Allocations
- Reuse payload buffers: construct `rmqt::Message` with a pre-sized `bsl::vector<uint8_t>` and mutate in place rather than re-allocating per send.
- Reuse routing keys/strings; avoid per-message string formatting. Keep `Topology`, `ExchangeHandle`, and `QueueHandle` stable to skip redeclare churn.
- Keep confirm callbacks lightweight (no heap allocations); if you must capture data, preallocate and store it alongside the producer.
- For consumers, reuse decoded buffers by processing in-place; avoid copying message bodies unless necessary.

## Minimizing Locks
- The fast path runs on one `io_context` thread; keep your application logic there to avoid cross-thread synchronization.
- Use single-thread pools for callbacks to avoid contention; minimize shared state touched by callbacks. The library still uses mutexes for lifecycle and channel/connection sequencing, so treat it as “low contention when single-threaded” rather than lock-free.
- There is no fully lock-free mode; atomics are used for counters, but some mutexes remain for connection/channel bookkeeping.

## Recording/Journal Hooks
- No built-in journal/recorder exists. To capture sent/received messages, attach:
  - `rmqp::ProducerTracing` / `rmqp::ConsumerTracing` for structured spans around sends and deliveries.
  - `rmqp::MetricPublisher` to emit metrics and payload sizes.
  - Custom transformers (see below) that log/duplicate payloads before publish and after delivery. Ensure logging stays non-blocking or offloads to another queue to avoid latency spikes.

## Compression (zstd)
- Build-time: `ENABLE_COMPRESSION=ON` (default) enables zstd support. Disable with `-DENABLE_COMPRESSION=OFF` if you want zero compression overhead.
- Producer side: `auto comp = rmqa::CompressionTransformer::create(); producer.addTransformer(comp.value());` to compress before publish.
- Consumer side: add the same transformer to `rmqt::ConsumerConfig::addTransformer` so inbound messages are decompressed automatically; otherwise consumers will see compressed payloads.
- When to use: helpful for large, text-heavy payloads over constrained networks. Avoid for small (<1–2 KB) or already-compressed data (images/zstd/zip) to prevent wasted CPU and latency.

## Throughput Tuning Tips
- Use higher `prefetchCount` in `ConsumerConfig` for better pipeline fill on high-latency links; balance against memory use.
- Set `maxOutstandingConfirms` high enough to keep the producer pipe full, but monitor for backpressure via confirm callbacks.
- Keep topology static and reuse connections/channels; churn causes extra round-trips and allocations.
- Use mandatory publishes (`RETURN_UNROUTABLE`) to detect routing issues quickly and avoid silent drops.
- Under network flaps, tune retry/watchdog: use `RabbitContextOptions::setShuffleConnectionEndpoints` to spread reconnects, and adjust watchdog/retry intervals (see `ConnectionMonitor`/`WatchDog`) if reconnect storms hurt latency.

## io_uring and External Event Sources
- When Asio is built with io_uring enabled, the same `io_context` used by `AsioEventLoop` will use io_uring internally; no code changes needed. Co-locate your other io_uring-aware Asio services on the same context to keep a single reactor.
- For epoll/inotify/eventfd/timerfd, wrap file descriptors with `boost::asio::posix::stream_descriptor` bound to the `context()` and let callbacks interleave with RabbitMQ traffic on one thread.

## Coroutines
- No native C++20 coroutine (`co_await`) API is provided. The public API uses callbacks and `rmqt::Future`. To integrate with coroutines, wrap `rmqt::Future` in a coroutine-friendly awaiter or bridge through `boost::asio::co_spawn`/`use_awaitable` on the same `io_context`.
- All async work is already on the Asio executor, so coroutine adapters should bind to `AsioEventLoop::context()` to avoid extra threads.

## External Usage Sightings
- Public GitHub search shows `bloomberg/rmqcpp` (this repo) and a small `arnoudvanleeuwen/rmqcpp` project. No other published users found yet; examples/tests here are the best reference for real-world patterns.
