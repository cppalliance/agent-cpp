# run_on Design

## Problem

`run_on(executor, task)` needs to:
1. Run the task on a specific executor
2. Dispatch completion back to the caller's executor
3. Handle executor lifetime safely (caller may not be a coroutine)

## Solution: `run_on_awaitable<T, E>`

A non-coroutine awaitable struct that:
- Stores executor **by value** (safe lifetime)
- Stores the task's coroutine handle
- Directly configures the task's promise when awaited

```cpp
template<typename T, executor E>
struct run_on_awaitable
{
    E ex_;
    std::coroutine_handle<typename task<T>::promise_type> h_;

    // 2-arg: co_await from coroutine
    template<dispatcher D>
    std::coroutine_handle<> await_suspend(coro continuation, D const& caller_ex)
    {
        h_.promise().ex_ = &ex_;              // Execute on our executor
        h_.promise().caller_ex_ = &caller_ex; // Complete to caller
        h_.promise().continuation_ = continuation;
        return h_;
    }

    // 1-arg: detached execution (no coroutine caller)
    // Precondition: 'this' is heap-allocated
    std::coroutine_handle<> await_suspend_detached()
    {
        h_.promise().ex_ = &ex_;
        h_.promise().caller_ex_ = &ex_;
        h_.promise().continuation_ = nullptr;
        h_.promise().detached_cleanup_ = +[](void* p) {
            delete static_cast<run_on_awaitable*>(p);
        };
        h_.promise().detached_state_ = this;
        return h_;
    }
};

template<executor E, typename T>
run_on_awaitable<T, E> run_on(E ex, task<T> t)
{
    return {std::move(ex), t.release()};
}

template<executor E, typename T>
void async_run(E ex, task<T> t)
{
    auto* state = new run_on_awaitable<T, E>{std::move(ex), t.release()};
    state->await_suspend_detached().resume();
}
```

## Task Promise Additions

```cpp
// In task<T>::promise_type:
void (*detached_cleanup_)(void*) = nullptr;
void* detached_state_ = nullptr;

// In final_suspend: if no continuation but has cleanup, call it
```

## Why This Design

| Aspect | Benefit |
|--------|---------|
| No extra coroutine frame | Zero allocation overhead |
| Executor by value | Safe lifetime regardless of caller |
| `co_await` lifetime extension | Awaitable stays alive during await |
| Heap allocation for detached | Self-destructs on completion |
| Direct promise access | No `set_executor` method needed |

## Usage

```cpp
// Coroutine caller - executor in awaitable, lifetime extended by co_await
co_await run_on(strand, some_task());

// Non-coroutine caller - heap allocated, self-destructs
async_run(strand, some_task());
```

## Key Insight

The caller knows the threading context, not the I/O object. Sockets should be executor-agnostic. `run_on` lets callers inject their executor choice into any task.
