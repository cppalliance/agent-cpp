## Context: Callback vs Coroutine Invocation Semantics in Asio

**Callbacks use immediate invocation on entry.** Asio examples show composed operations launched directly from constructors or completion handlers without dispatch/post. The caller is trusted to understand the execution context.

**Coroutines use post/dispatch on entry.** `co_spawn` implementation (1.74.0+) begins with `co_await post(executor, ...)` before executing user code. Later versions (1.80+) changed to `dispatch` for optimization when already in correct context.

**Rationale for difference:** Coroutine frames may be allocated and reference executor-specific state before any user code runs. Asio ensures safety by forcing execution onto the correct executor before the coroutine body begins. Callbacks have no such intermediate state - the initiating function runs synchronously and only completions route through the executor.

**Implication for coroutine library design:** To match callback performance characteristics, provide two launch mechanisms:
1. A safe launch (`dispatch_io`) that posts/dispatches for initial entry from unknown contexts
2. An immediate launch (`invoke_io`) for subsequent operations within a chain where the executor context is already correct

Once the first async operation completes, the chain remains on the correct executor indefinitely - matching callback behavior where all work after initial launch executes in `io_context::run()` threads.
