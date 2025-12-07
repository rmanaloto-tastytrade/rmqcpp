# Experimental Features

The build flag `USES_LIBRMQ_EXPERIMENTAL_FEATURES` enables opt-in APIs that may change without notice. These are available in `rmq`/`rmqa` and some integration binaries. Use only if you can tolerate churn.

## Enabled APIs
- `rmqa::Producer::trySend` — non-blocking variant that returns `INFLIGHT_LIMIT` instead of blocking when outstanding confirms hit the limit. Without the flag, only `send` is available.
- `rmqa::VHost::deleteQueue` — queue deletion with `ifUnused`/`ifEmpty` options and timeout.
- `rmqa::VHost::createProducerAsync` / `createConsumerAsync` — async creation helpers returning `rmqt::Future`.
- Integration samples (`librmq_queuedelete`, `librmq_exitboth`, `rmqperftest`, `rmqmonitor`) and the `rmq` aggregate target are compiled with this flag for demo/testing purposes.

## Usage Notes
- Binary and API stability are not guaranteed; methods may be renamed, moved, or removed.
- Keep call sites localized so you can refactor easily if the experimental surface changes.
- If you do not define the flag, the experimental methods are not compiled in and must not be referenced.
