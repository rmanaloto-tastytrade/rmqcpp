# rmqtestmocks Tests

Sanity checks for the gmock helpers. `rmqtestmocks_tests` links only the mock library and GTest/GMock to keep dependency surface minimal.

## Cases
- `rmqtestmocks_mocks.t.cpp` — verifies default expectations and that mocks surface the same signatures as the public `rmqp` interfaces.

## Notes
- Lightweight because protocol/IO stacks are not required when exercising mock expectations.
