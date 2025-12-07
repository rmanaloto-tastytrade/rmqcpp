# rmqamqpt (AMQP Protocol Types)

Low-level AMQP 0-9-1 primitives: frames, buffers, and method structures. These classes mirror the protocol spec so higher layers can encode/decode without duplicating parsing logic.

## Core Types
- `Frame`, `ContentHeader`, `ContentBody`, `Heartbeat` — frame containers with channel ids and size handling.
- `Buffer` — byte cursor for serialization/deserialization.
- `FieldValue`, `Types` — AMQP field value encoding (tables, arrays, strings, decimals, booleans, timestamps).
- `Writer` — helper for writing frames to buffers.
- `Method` and method category bases (`BasicMethod`, `ChannelMethod`, `ConnectionMethod`, `ExchangeMethod`, `QueueMethod`, `ConfirmMethod`) — provide polymorphic dispatch for each AMQP class.
- `Constants` — AMQP class/method ids, reply codes, and magic numbers.

## Method Structures
Each AMQP operation has a dedicated class with encode/decode helpers. Examples include:
- Basic: `BasicAck`, `BasicCancel`, `BasicCancelOk`, `BasicConsume`, `BasicConsumeOk`, `BasicDeliver`, `BasicNack`, `BasicProperties`, `BasicPublish`, `BasicQos`, `BasicQosOk`, `BasicReturn`.
- Channel: `ChannelOpen`, `ChannelOpenOk`, `ChannelClose`, `ChannelCloseOk`, `ChannelFlow`, `ChannelFlowOk`.
- Connection: `ConnectionStart`, `ConnectionStartOk`, `ConnectionTune`, `ConnectionTuneOk`, `ConnectionOpen`, `ConnectionOpenOk`, `ConnectionClose`, `ConnectionCloseOk`.
- Confirm: `ConfirmSelect`, `ConfirmSelectOk`.
- Exchange: `ExchangeDeclare`, `ExchangeDeclareOk`, `ExchangeBind`, `ExchangeBindOk`.
- Queue: `QueueDeclare`, `QueueDeclareOk`, `QueueBind`, `QueueBindOk`, `QueueDelete`, `QueueDeleteOk`, `QueueUnbind`, `QueueUnbindOk`.

## Dependencies
- Consumed by `rmqamqp` for frame production/consumption and by `rmqio` for decoding serialized frames.
- Pure protocol logic; no event loop assumptions, so it works the same whether the `io_context` is backed by epoll, kqueue, or io_uring.

## Entry Points
- Typically accessed through `rmqamqp::Framer`/`ContentMaker` when turning `rmqt` types into protocol frames or decoding replies.
