# Extending the Affine Protocol: Multi-Capability Coordination

This document analyzes how to extend `await_suspend` with multiple capabilities (dispatcher, stop token, etc.) using an environment-based approach aligned with P2300.

---

## Problem Statement

### The Core Conflict

The affine awaitables proposal extends `await_suspend` with a second parameter:

```cpp
auto await_suspend(std::coroutine_handle<> h, Dispatcher const& d);
```

If another proposal (e.g., for stop token propagation) uses the same extension point:

```cpp
auto await_suspend(std::coroutine_handle<> h, StopToken const& st);
```

**An awaitable cannot opt into both protocols simultaneously.** Both claim the same parameter position with incompatible types. Each new capability doubles the overload set—with N capabilities, you need 2^N overloads.

### Why This Matters

1. **Protocol incompatibility**: Two valuable protocols cannot coexist in the same awaitable
2. **Ecosystem fragmentation**: Libraries must choose which protocol to support
3. **Future-proofing failure**: Any new capability (allocator, priority, deadline, etc.) faces the same problem

---

## Solution: Environment Object

Instead of separate parameters, pass a single environment that provides multiple capabilities:

```cpp
auto await_suspend(std::coroutine_handle<> h, Env const& env);
```

The awaitable queries for what it needs:

```cpp
template<typename Env>
auto await_suspend(std::coroutine_handle<> h, Env const& env) {
    if constexpr (requires { get_dispatcher(env); }) {
        auto& d = get_dispatcher(env);
        // use dispatcher for affine resumption
    }
    if constexpr (requires { get_stop_token(env); }) {
        auto& st = get_stop_token(env);
        // register for cancellation
    }
}
```

**Benefits:**
- One parameter forever, infinitely extensible
- Awaitables only pay for what they use
- Aligns with P2300's receiver/environment pattern

---

## Precedent: P3826R2

[P3826R2](https://wg21.link/P3826) ("Fix or Remove Sender Algorithm Customization") addresses a parallel problem in the sender/receiver world.

P3826 identifies that senders face the same "blind context" issue:

> "Many senders do not know where they will complete until they know where they will be started."

The fix: **pass the receiver's environment when querying the sender**:

```cpp
// Old (broken): sender doesn't know its context
auto dom = get_domain(get_env(sndr));

// New (fixed): tell sender where it will start
auto dom = get_completion_domain<set_value_t>(get_env(sndr), get_env(rcvr));
```

### Key Findings

1. **Environment queries with additional arguments are sanctioned.** P3826 Section 4.1.1: "A query that accepts an additional argument is novel in std::execution, but **the query system was designed to support this usage**."

2. **The query mechanism is defined.** P3826 changes `env<>::query` to accept variadic arguments.

3. **Multiple orthogonal queries coexist.** P3826 adds `get_completion_domain<set_value_t>`, `get_completion_domain<set_error_t>`, and `get_completion_domain<set_stopped_t>`—demonstrating that dispatcher + stop token can coexist.

---

## Implementation

### Query CPOs

```cpp
// Customization point objects (would be standardized)
inline constexpr struct get_dispatcher_t {
    template<typename Env>
    auto operator()(Env const& env) const 
        -> decltype(env.query(get_dispatcher_t{})) {
        return env.query(get_dispatcher_t{});
    }
} get_dispatcher{};

inline constexpr struct get_stop_token_t {
    template<typename Env>
    auto operator()(Env const& env) const 
        -> decltype(env.query(get_stop_token_t{})) {
        return env.query(get_stop_token_t{});
    }
} get_stop_token{};
```

### Environment Type

```cpp
// Environment bundling multiple capabilities
template<typename Dispatcher, typename StopToken>
struct awaitable_env {
    Dispatcher const* dispatcher_;
    StopToken const* stop_token_;

    // Query interface (P2300-style)
    auto query(get_dispatcher_t) const noexcept -> Dispatcher const& {
        return *dispatcher_;
    }
    auto query(get_stop_token_t) const noexcept -> StopToken const& {
        return *stop_token_;
    }
};
```

### Environment-Injecting Awaiter

```cpp
// Awaiter that injects the environment into await_suspend
template<typename Awaitable, typename Env>
struct env_awaiter {
    Awaitable awaitable_;
    Env env_;

    bool await_ready() { 
        return awaitable_.await_ready(); 
    }

    template<typename Promise>
    auto await_suspend(std::coroutine_handle<Promise> h) {
        // Pass environment to awaitable if it accepts it
        if constexpr (requires { awaitable_.await_suspend(h, env_); }) {
            return awaitable_.await_suspend(h, env_);
        } else {
            // Fallback for legacy awaitables
            return awaitable_.await_suspend(h);
        }
    }

    decltype(auto) await_resume() { 
        return awaitable_.await_resume(); 
    }
};
```

### Promise with await_transform

```cpp
template<typename T>
struct promise_type {
    // Storage for capabilities
    some_dispatcher dispatcher_;
    std::stop_token stop_token_;

    template<typename Awaitable>
    auto await_transform(Awaitable&& a) {
        using A = std::remove_cvref_t<Awaitable>;
        
        // Create environment bundling both capabilities
        using Env = awaitable_env<some_dispatcher, std::stop_token>;
        Env env{&dispatcher_, &stop_token_};

        return env_awaiter<A, Env>{
            std::forward<Awaitable>(a),
            env
        };
    }

    // ... rest of promise
};
```

### Awaitable Implementation

An awaitable that wants both capabilities queries the environment:

```cpp
struct my_async_operation {
    bool await_ready() { return false; }

    template<typename Env>
    auto await_suspend(std::coroutine_handle<> h, Env const& env) {
        // Query for dispatcher (required for affine)
        auto& dispatcher = get_dispatcher(env);
        
        // Query for stop token (optional)
        if constexpr (requires { get_stop_token(env); }) {
            auto& token = get_stop_token(env);
            // Register cancellation callback...
        }

        // Start async work, resume via dispatcher
        start_async_work([h, &dispatcher]() {
            dispatcher(h);  // Resume through dispatcher
        });

        return std::noop_coroutine();
    }

    int await_resume() { return result_; }
};
```

### Extensibility

Adding a third capability (e.g., allocator) requires no changes to existing awaitables:

```cpp
template<typename Dispatcher, typename StopToken, typename Allocator>
struct extended_env {
    Dispatcher const* dispatcher_;
    StopToken const* stop_token_;
    Allocator const* allocator_;

    auto query(get_dispatcher_t) const noexcept { return *dispatcher_; }
    auto query(get_stop_token_t) const noexcept { return *stop_token_; }
    auto query(get_allocator_t) const noexcept { return *allocator_; }
};
```

Existing awaitables that only query for `get_dispatcher` and `get_stop_token` continue to work unchanged.

---

## Revised Protocol

```cpp
// Environment concept (aligned with P2300)
template<typename E>
concept awaitable_env = requires(E const& e) {
    { get_dispatcher(e) } -> dispatcher;
};

// Affine awaitable concept (updated)
template<typename A, typename E, typename P = void>
concept affine_awaitable =
    awaitable_env<E> &&
    requires(A a, std::coroutine_handle<P> h, E const& env) {
        a.await_suspend(h, env);
    };
```

---

## Next Steps

1. **Coordinate with P2300 authors**: Ensure the environment pattern aligns with `std::execution`'s direction
2. **Engage with stop token proposal authors**: Converge on a unified environment
3. **Consider a foundational paper**: The environment concept for awaitables may warrant its own proposal
4. **Evaluate `queryable` integration**: Determine if affine environments should satisfy P2300's `queryable` concept

---

## References

- [P3826R2](https://wg21.link/P3826) — Fix or Remove Sender Algorithm Customization
- [P2300R10](https://wg21.link/P2300R10) — `std::execution`
- [P3325R1](https://wg21.link/P3325R1) — A Utility for Creating Execution Environments
