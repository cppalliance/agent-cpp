## Additional Feedback (2025-12-31)

### Key Concerns

1. **await_transform Overload Specificity**: Many promise types have multiple `await_transform` overloads (e.g., `co_await asio::this_coro::executor` returns a noop awaitable). The paper templates `await_transform` making everything affine, but libraries need a concept to determine what should be made affine. Suggested pattern:
   ```cpp
   template<std::execution::sender Sender>
   auto await_transform(Sender&& sndr) noexcept {
       return as_awaitable(affine_on(std::forward<Sender>(sndr), SCHED(*this)), *this);
   }
   
   template<affine_awaitable<context_type> A>
   auto await_transform(A&& a);
   ```
   This allows libraries like Boost.Cobalt or Asio to add support without modifying existing awaitables, and enables compile-time rejection of non-affine awaitables for zero-allocation guarantees.

2. **Dispatcher Concept Usage**: The `dispatcher` concept exists in the paper but isn't used anywhere. Should be used in constraints. Suggested concept:
   ```cpp
   template<typename D, typename P = void>
   concept dispatcher = requires (D d, std::coroutine_handle<P> h) {{d(h)};};
   ```
   This is important if a library author already has a second parameter for `await_suspend` and needs to distinguish it.

3. **Simplification Opportunity**: Can simplify `d([h] { h.resume(); })` to just `d(h)` since `std::coroutine_handle<>` has an `operator()`. This was already incorporated.

4. **Concept Enables Both Approaches**: The `affine_awaitable` concept enables:
   - **Tiered mode**: Compile-time detection with fallback (paper's emphasis on compatibility)
   - **Strict mode**: Compile-time rejection for zero-allocation guarantees (colleague's use case)
   Both are valid uses of the same primitive.

### Implementation Notes

- The dispatcher being passed by `const&` (not `&&`) was questioned, but confirmed as correct in the protocol.
- Buffer-based allocation for `make_affine` was discussed but deemed not viable for standardization.
- The colleague confirmed the paper looks fine overall, with these details as improvements.

---

## Additional Content from Paper Sections 7.1-7.6

### §7.1 Non-Breaking Overload Addition

Adding `await_suspend(h, d)` alongside existing `await_suspend(h)` is completely non-breaking. The following table shows which overload is selected in each context:

| Context | Awaitable Type | Overload Selected |
|---------|---------------|-------------------|
| Legacy awaitable (only `await_suspend(h)`) | Non-affine | `await_suspend(h)` (legacy path) |
| Affine awaitable (both overloads) | Affine | `await_suspend(h, d)` (affine path) |
| Legacy code awaiting affine awaitable | Affine | `await_suspend(h)` (backward compatible) |
| Affine-aware code awaiting legacy awaitable | Non-affine | `await_suspend(h)` (fallback to legacy) |

This ensures that existing code continues to work unchanged, while new affine-aware code can opt into zero-allocation affinity.

### §7.2 Strict Mode: Compile-Time Enforcement

Strict mode uses `await_transform` constrained with `static_assert` to reject non-affine awaitables at compile time, providing the zero-allocation guarantee:

```cpp
template<affine_awaitable<context_type> A>
auto await_transform(A&& a) {
    return affine_awaiter{std::forward<A>(a), &dispatcher_};
}

template<typename A>
auto await_transform(A&& a) {
    static_assert(affine_awaitable<std::remove_cvref_t<A>, context_type>,
                  "Only affine awaitables are accepted in strict mode");
    return affine_awaiter{std::forward<A>(a), &dispatcher_};
}
```

This compile-time enforcement ensures that non-affine awaitables are rejected before code generation, providing a hard guarantee of zero allocations.

### §7.3 Tiered vs. Strict: Same Primitives, Different Policies

The `affine_awaitable` concept enables two distinct usage patterns with the same primitives:

| Approach | Policy | Use Case |
|----------|--------|----------|
| **Tiered (§6)** | Accept all, optimize what we can | Library adoption, gradual migration |
| **Strict (§7.2)** | Reject non-affine at compile time | Performance-critical systems, embedded |

Both approaches use the same `affine_awaitable` concept and protocol—the difference is in the policy enforced by `await_transform`:
- **Tiered mode**: Uses `if constexpr` to detect affine awaitables and fall back to `make_affine` for non-affine ones
- **Strict mode**: Uses `static_assert` to reject non-affine awaitables at compile time

### §7.4 Gradual Library Evolution

The concept serves as a migration tool with three phases:

**Phase 1: Status quo**
- Works everywhere
- 1 allocation fallback for non-affine awaitables via `make_affine`

**Phase 2: Add affine overload**
- Non-breaking addition of `await_suspend(h, d)` to existing awaitables
- Zero-allocation for affine callers
- Legacy callers continue to work unchanged

**Phase 3: Remove legacy overload**
- Compile-time enforcement via `static_assert`
- Zero-allocation guarantee across the entire chain
- Requires all awaitables in the chain to be affine

This phased approach allows libraries to migrate incrementally without breaking existing code.

### §7.5 Example: Strict-Mode Task

Complete `strict_task` implementation demonstrating compile-time enforcement:

```cpp
template<typename T>
class strict_task {
    struct promise_type {
        Dispatcher const* dispatcher_ = nullptr;
        
        template<affine_awaitable<Dispatcher> A>
        auto await_transform(A&& a) {
            return affine_awaiter{std::forward<A>(a), dispatcher_};
        }
        
        template<typename A>
        auto await_transform(A&& a) {
            static_assert(affine_awaitable<std::remove_cvref_t<A>, Dispatcher>,
                          "strict_task only accepts affine awaitables");
            // Unreachable, but satisfies return type requirement
            return affine_awaiter{std::forward<A>(a), dispatcher_};
        }
        
        // ... rest of promise implementation
    };
    
    // ... rest of task implementation
};
```

This ensures that any attempt to `co_await` a non-affine awaitable in a `strict_task` results in a compile-time error.

### §7.6 Migration Tool, Not Just Detection

The concept serves a dual purpose that should be explicitly reframed:

**Detection (tiered mode)**: `if constexpr` selection
- Detects whether an awaitable is affine at compile time
- Falls back to `make_affine` for non-affine awaitables
- Enables gradual migration and library compatibility

**Enforcement (strict mode)**: `static_assert` guarantee
- Rejects non-affine awaitables at compile time
- Provides hard zero-allocation guarantee
- Enables performance-critical and embedded use cases

The same `affine_awaitable` concept enables both patterns—it's not just about detection, but about providing a migration path from today's ecosystem to zero-overhead scheduler affinity.


