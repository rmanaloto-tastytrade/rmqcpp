# Examples

Reference examples under `examples/` with links to detailed notes in each directory.

- [`examples/helloworld`](../examples/helloworld/README.md) — minimal producer publishing one message and waiting for confirms on the shared Asio event loop.
- [`examples/topology`](../examples/topology/README.md) — headers-exchange example with two consumers sharing the same `io_context`.
- [`examples/rmqperftest`](../examples/rmqperftest/README.md) — performance harness and CLI knobs.
- [`examples/monitor`](../examples/monitor/monitor.m) — work-in-progress monitor for event exchange, firehose, metadata store, and log exchange on a single `io_context`; uses Boost.Beast for management API polling and Quill for logging.

## Event Loop Considerations
All examples run on [`rmqio::AsioEventLoop`](../src/rmq/rmqio/README.md) and therefore can share a single-threaded [`boost::asio::io_context`](https://www.boost.org/doc/libs/latest/doc/html/boost_asio.html) with other async workloads (epoll/io_uring timers, sockets, REST clients). Hook your own Asio services into the same context if you need coordinated scheduling.
