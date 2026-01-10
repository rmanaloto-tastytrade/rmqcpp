# rmqtestutil Tests & Library

`rmqtestutil` is both a utility library (mock timers/resolvers/metrics) and a small test suite to ensure replay helpers work. The library links `bsl`, `bdl`, `rmqamqpt`, `rmqamqp`, `rmqt`, and GMock/GTest; the tests link back to the library for replay coverage.

## Files
- Utility sources: `rmqtestutil_callcount`, `clockoverride`, `mockchannel`, `mockeventloop`, `mockmetricpublisher`, `mockresolver`, `mockretryhandler`, `mocktimerfactory`, `replayframe`, `savethreadid`, `timedmetric` — provide fakes for timing, channels, resolvers, retry strategies.
- Tests: `rmqtestutil_replayframe.t.cpp` exercises replay of serialized frames.

## Notes
- Designed to plug into `rmqio`/`rmqamqp` unit tests to control time and scheduling; works with any `io_context`-backed event loop because it replaces timers/resolvers rather than sockets.
