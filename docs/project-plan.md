# Project Plan (current priorities)

- **Top priority:** Modern C++26 RabbitMQ admin client (rabbitmqadmin replacement). See `docs/rabbitmqadmin-modern.md`.
- Enforce clang-22 + libc++ toolchain across presets; keep CMake/libc++ checks green.
- Continue monitor example hardening (single-threaded Asio loop, management polling TBD).
- Documentation maintenance: ensure new admin client and monitor docs are linked from README/docs once stabilized.
