**Document number:** PXXXXR0  
**Date:** 2025-12-30  
**Reply-to:** Vinnie Falco \<vinnie.falco@gmail.com\>  
**Audience:** SG1, LEWG  

---

# Affine Awaitables : Zero-Overhead Scheduler Affinity

## Abstract

This document proposes a minimal protocol for zero-overhead scheduler affinity in C++ coroutines. By introducing the `dispatcher` and
`affine_awaitable` concepts, awaitables can opt into zero-allocation scheduler affinity without requiring the full sender/receiver protocol. The protocol is library-implementable today; standardization would bless the concepts and enable ecosystem-wide adoption.

---

## 1. The Lost Context Problem

Consider a common scenario: a coroutine running on the UI thread needs to fetch data from the network.

```cpp
task ui_handler() {               // Runs on UI thread
    auto data = co_await fetch(); // fetch() completes on network thread
    update_ui(data);              // BUG: Where are we now?
}
```

When `fetch()` completes, the network subsystem resumes our coroutine—but on the *network* thread, not the UI thread. The call to `update_ui()` races, crashes, or corrupts state. This is the scheduler affinity problem: after any `co_await`, how does a coroutine resume on its *home* scheduler?

The challenge deepens when coroutines call other coroutines:

```cpp
task<Data> fetch_and_process() {  // Started on UI thread
    auto raw = co_await network_read();   // Completes on network thread
    auto parsed = co_await parse(raw);    // Where does parse() run?
    co_return transform(parsed);          // Where does this run?
}
```

Every suspension point is an opportunity to get lost.

---

## 2. Today's Solutions, and Costs

P3552 proposes coroutine tasks with scheduler affinity—ensuring a coroutine resumes on its designated scheduler after any `co_await`. Several approaches exist:

### 2.1 Manual Discipline

```cpp
task ui_handler() {
    auto data = co_await fetch();
    co_await resume_on(ui_scheduler);  // Programmer must remember
    update_ui(data);
}
```

**Problem:** Error-prone. Every `co_await` is a potential bug.

### 2.2 Sender Composition

```cpp
task ui_handler() {
    auto data = co_await continues_on(fetch(), ui_scheduler);
    update_ui(data);  // Guaranteed: UI thread
}
```

**Problem:** Only works with P2300 senders, excluding the ecosystem of awaitables (Asio, custom libraries). This path makes unproven assumptions that senders will be ideal for all future, undiscovered use-cases.

### 2.3 Universal Trampoline

Our helper `make_affine` wraps any awaitable in a coroutine to provide scheduler affinity. The trampoline awaits the non-affine awaitable, then dispatches the continuation through the caller's dispatcher. This enables scheduler affinity for awaitables that do not participate in the affine protocol. The reference implementation includes `[[clang::coro_await_elidable]]` to enable HALO optimization when possible; see [`make_affine.hpp`](https://github.com/cppalliance/corosio/blob/master/include/capy/make_affine.hpp).

> **Note:** `make_affine` is not proposed for standardization. It is provided as an example implementation technique in the reference implementation. This proposal only standardizes the protocol itself (§4) and the concepts that detect it (§4.1, §4.2).

```cpp
// Works with ANY awaitable
auto data = co_await make_affine(fetch(), dispatcher);
```

**Problem:** Typically allocates on every `co_await`. The trampoline coroutine handle escapes to the dispatcher, which may prevent HALO optimization from applying. HALO is unreliable across compilers.

> **Note on HALO reliability:** Testing shows HALO is far more limited than often assumed. MSVC (19.50) does not implement HALO at all. Clang elides only the *outermost* coroutine frame; nested coroutines still allocate. The affine protocol provides the only *reliable* zero-allocation path.

### 2.4 Why This Matters Now

P3552 (`std::execution::task`) is about to be standardized and makes significant assumptions about sender/receiver's universal applicability to asynchronous workloads. The framework has demonstrable value for GPU and parallel workloads, where its design strengths align with the problem domain. However, for CPU-bound I/O workloads such as networking, the committee has not yet explored the use case. Early studies by the authors suggest that sender APIs may not be optimal for networking's particular needs.

Our design is simpler, less risky, and provides a foundation upon which a standard task type can be built. The broader ecosystem which does not currently use senders, and future user code which may not need senders, would benefit from our proposal:

**Ecosystem compatibility.** Boost.Asio has active coroutine support. Libraries like libcoro, folly::coro, and Bloomberg's Quantum are actively maintained and power real systems today. Countless custom awaitables exist in proprietary codebases. These libraries represent proven, stable infrastructure that developers rely on. Affine awaitables let this infrastructure benefit from scheduler affinity without requiring conversion to the sender/receiver protocol.

**Incremental adoption.** Not every codebase needs the full compositional power of senders. Many applications simply need coroutines that resume on the correct thread. The affine protocol (~10 lines) provides a lower barrier to entry than the full sender/receiver protocol (~100+ lines), allowing teams to adopt scheduler affinity immediately and evolve toward senders as their needs grow.

**No-regret design.** Affine awaitables integrate seamlessly with P2300—senders flow through the same `await_transform` and receive optimal handling. Whether senders become ubiquitous or adoption is slower than expected, affine awaitables ensure scheduler affinity is available today and remain useful for custom awaitables and legacy integration.

**Layered abstraction.** The dispatcher concept is more general than the scheduler concept—every scheduler can produce a dispatcher, but not every dispatcher requires the full sender/receiver protocol. This positions affine awaitables as a foundation that sender-based designs can build upon (see §11).

---

## 3. The Solution: One Parameter

The allocation in `make_affine` happens because it requires an extra coroutine (the trampoline) to intercept and redirect resumption. The awaitable doesn't know where to resume—it hands back control blindly, and the trampoline coroutine must be created to handle the dispatch, with whatever overhead that brings.

The fix is simple: tell the awaitable where to resume.

```cpp
// Standard: awaitable is blind
void await_suspend(std::coroutine_handle<> h);

// Extended: awaitable knows the dispatcher
void await_suspend(std::coroutine_handle<> h, Dispatcher const& d);
```

One additional parameter eliminates the allocation.

The awaitable receives the dispatcher and uses it directly:

```cpp
template<typename Dispatcher>
auto await_suspend(std::coroutine_handle<> h, Dispatcher const& d) {
    return d(h);  // Resume through dispatcher, return handle for symmetric transfer
}
```

No trampoline. No allocation. The awaitable handles affinity internally.

### 3.1 Propagation

However, `await_suspend(h, d)` only tells the awaitable how to be dispatched—it does not propagate affinity through nested coroutine chains. Consider:

```cpp
task<int> outer() {
    auto result = co_await inner();  // inner() is also a task
    co_return result;
}

task<int> inner() {
    auto data = co_await fetch();  // Resumes on unknown context
    co_return data;
}
```

If `inner()` is a coroutine task that doesn't implement the propagation rules, affinity is lost when `inner()` awaits other operations. The `fetch()` operation completes and resumes `inner()` on an unknown context, breaking the affinity chain. Our proposal includes guidance for propagating affinity that does not require any additional standard library support. It is a set of rules that awaitable authors can follow to propagate affinity at zero cost for any affine awaitable. By offering rules instead of standardese, authors have flexibility in implementation (see §4.4).

---

## 4. The Affine Awaitable Protocol

The protocol outlined in this section gives authors tools to solve both aspects of the lost context problem: awaitable affinity, and affinity propagation.

- **As an Awaitable**: You implement the protocol to respect the Caller's affinity (resume them where they want).

- **As a Task Type**: You implement the promise logic to enforce Your affinity (resume yourself where you want).

This protocol is fully implementable as a library today; standardization would bless the concepts and reduce boilerplate, without requiring `std::execution::task`. Dispatchers can also carry extra policy (e.g., priority save/restore) without affecting the core protocol.

### 4.1 Dispatcher Concept

A **dispatcher** accepts a coroutine handle for resumption and returns a coroutine handle for symmetric transfer:

```cpp
template<typename D, typename P = void>
concept dispatcher = requires(D const& d, std::coroutine_handle<P> h) {
    { d(h) } -> std::convertible_to<std::coroutine_handle<>>;
};
```

*Remarks:* The dispatcher must return a `std::coroutine_handle<>` (or convertible type) to enable symmetric transfer. Calling `d(h)` schedules `h` for resumption (typically on a specific execution context) and returns a coroutine handle that the caller may use for symmetric transfer. The dispatcher must be const-callable (logical constness), enabling thread-safe concurrent dispatch from multiple coroutines. Since `std::coroutine_handle<P>` has `operator()` which invokes `resume()`, the handle itself is callable and can be dispatched directly.

*Lifetime:* In `await_suspend`, the dispatcher is received by const lvalue reference. The callee treats it as *borrowed for the duration of the await* (do not retain past completion). Ownership and lifetime are the caller's responsibility. The caller may store the dispatcher by value (if owning) or by pointer/reference (if externally owned) but must present it to callees as a const lvalue reference.

### 4.2 Affine Awaitable Concept

An awaitable is *affine* if it accepts a dispatcher in `await_suspend`:

```cpp
template<typename A, typename D, typename P = void>
concept affine_awaitable =
    dispatcher<D, P> &&
    requires(A a, std::coroutine_handle<P> h, D const& d) {
        a.await_suspend(h, d);
    };
```

*Remarks:* This concept detects awaitables that participate in the affine protocol by providing the extended `await_suspend(h, d)` overload. The dispatcher is passed by const lvalue reference, and the awaitable must use it to resume the caller.

### 4.3 Awaitable Requirements (callee role)

An awaitable **participates in the affinity protocol** if it provides an overload:

```cpp
template<class Dispatcher>
auto await_suspend(std::coroutine_handle<> h, Dispatcher const& d) {
    return d(h);  // Possible implementation: resume caller via dispatcher, return handle for symmetric transfer
}
```

Semantics:
- The awaitable must use the dispatcher `d` to resume the caller, e.g. `return d(h);`.
- The dispatcher returns a coroutine handle that `await_suspend` may return for symmetric transfer.
- It may run its own work on any context; only resumption must use `d`.

### 4.4 Task/Promise Requirements (caller role)

A task type (its promise) **propagates affinity** if it:

1) **Stores the caller's dispatcher.**  
   - Provides a way to set/hold a dispatcher instance for the lifetime of the promise.

2) **Forwards the dispatcher on every await.**  
   - In `await_transform`, when awaiting `A`, pass the stored dispatcher to the awaited object via an affine awaiter:
     ```cpp
     // dispatcher_handle: whatever you store for the caller's dispatcher (pointer or reference)
     return affine_awaiter{std::forward<Awaitable>(a), dispatcher_handle};
     ```
   - The callee receives it as `Dispatcher const&` in `await_suspend(h, d)`.
   - If `A` is not affine and you still want affinity, wrap it (e.g. `make_affine(A, dispatcher)`) or reject it at compile time.

3) **Provides the affine await path for its own awaitability.**  
   - `await_suspend(caller, dispatcher)` (affine, dispatcher available) that stores dispatcher + continuation before resuming the task's coroutine.
   - Optionally provides `await_suspend(caller)` (legacy, no dispatcher) for backward compatibility with non-affine callers. Strict-mode tasks may omit this overload, rejecting non-affine callers at compile time.

   In the affine path, `await_suspend(caller, d)` typically:
   - Stores the continuation handle (caller) and the dispatcher `d` in the promise, then
   - Returns the task's coroutine handle to start execution, enabling later forwarding (via `await_transform`) and final resumption via the stored dispatcher.

4) **Final resumption uses the dispatcher when set.**  
   - In `final_suspend`, if a dispatcher is stored, resume the continuation via that dispatcher; otherwise use direct symmetric transfer.

### 4.5 Propagation Rule

- The dispatcher is set once at the top-level task (e.g., `run_on(ex, task)`).
- Each `co_await` forwards the same dispatcher to the awaited object.
- Any awaited object that implements `await_suspend(h, d)` uses that dispatcher to resume its caller, preserving affinity through arbitrary nesting.
- If an awaited object is non-affine and not adapted, affinity may be lost at that point. A trampoline (e.g., `make_affine`) can restore it; alternatively, implementations may reject non-affine awaits at compile time.

---

## 5. Reference Implementation

The protocol defined in §4 can be implemented entirely as a library. A complete reference implementation is available at:

https://github.com/cppalliance/corosio

The implementation includes:

- **`task<T>`** — A coroutine task type implementing the affine awaitable protocol
- **`make_affine`** — A trampoline wrapper for legacy awaitables that don't implement the protocol
- **`any_dispatcher`** — A type-erased dispatcher for runtime polymorphism
- **`async_run`** — Entry point for launching detached tasks with a dispatcher
- **`run_on`** — Binds a task to execute on a specific dispatcher

---

## 6. Benefits

**Zero-Overhead Affinity**

Both P2300 senders and affine awaitables achieve zero allocation per `co_await`—crucially, without depending on HALO. However, senders require conversion of the entire awaitable ecosystem (~100+ lines per awaitable, high expertise with the sender/receiver protocol), while affine awaitables work with existing awaitables through a single overload (~10 lines, low expertise). Legacy awaitables that don't participate in the protocol lose affinity entirely, while `make_affine` provides a fallback for foreign awaitables.

**Mitigating Risk**

While senders demonstrate clear value for GPU and parallel workloads, networking represents an unexplored use case in committee discussions. Early studies suggest sender APIs may not align optimally with networking's particular needs—high-frequency, low-latency I/O operations with different composition patterns than parallel algorithms. Adopting senders as the sole path commits the ecosystem to an unproven design for networking workloads. Affine awaitables provide a proven, minimal foundation that works today and remains valuable regardless of sender adoption outcomes, offering lower risk for networking applications.

**Implementation Trade-offs**

The affine protocol provides zero-overhead affinity with minimal implementation burden, making it accessible to the entire awaitable ecosystem. Senders offer pipe composition but require ecosystem conversion; affine awaitables provide affinity propagation without requiring conversion. Both approaches require opt-in, but affine awaitables integrate seamlessly with existing code—Boost.Asio, libcoro, folly::coro, and countless custom awaitables can adopt the protocol incrementally. The minimal cost (~10 lines per awaitable) enables immediate adoption while preserving flexibility for future evolution toward senders if desired.

**Incremental Adoption Path**

The protocol enables incremental adoption:

1. **Library authors:** Add `await_suspend(h, dispatcher)` overload to existing awaitables. This is non-breaking—existing code continues to work, and affine-aware tasks gain zero-allocation affinity.

2. **Task authors:** Implement the protocol requirements (§4.4) in their promise types to propagate affinity through `co_await` chains.

3. **Foreign awaitables:** For awaitables that cannot be modified (third-party libraries, legacy code), authors can use `make_affine` to opt them into scheduler affinity. This tradeoff may be acceptable when the awaitable is infrequently used or when scheduler affinity is more valuable than the allocation cost.

4. **Ecosystem evolution:** As more awaitables adopt the protocol, the benefits compound. Libraries can choose to require affine awaitables (rejecting non-affine at compile time) or accept non-affine awaitables with appropriate fallbacks.

The protocol is minimal (~10 lines per awaitable), library-implementable, and provides a clear migration path from today's awaitable ecosystem to zero-overhead scheduler affinity.

> **Note (non-normative):** Future awaitables added to the standard library should implement the affine awaitable protocol to enable zero-overhead scheduler affinity. This ensures that standard library awaitables work optimally with scheduler-affine task types.

### 6.1 Implementation Patterns (Non-Normative)

The `affine_awaitable` concept enables multiple implementation patterns, allowing library authors to choose the policy that best fits their use case. This section describes non-normative implementation guidance.

#### 6.1.1 Non-Breaking Overload Addition

Adding `await_suspend(h, d)` alongside existing `await_suspend(h)` is completely non-breaking. The compiler automatically selects the appropriate overload based on availability:

| Context | Awaitable Type | Overload Selected |
|---------|---------------|-------------------|
| Legacy awaitable (only `await_suspend(h)`) | Non-affine | `await_suspend(h)` (legacy path) |
| Affine awaitable (both overloads) | Affine | `await_suspend(h, d)` (affine path) |
| Legacy code awaiting affine awaitable | Affine | `await_suspend(h)` (backward compatible) |
| Affine-aware code awaiting legacy awaitable | Non-affine | `await_suspend(h)` (fallback to legacy) |

This ensures that existing code continues to work unchanged, while new affine-aware code can opt into zero-allocation affinity.

#### 6.1.2 Tiered vs. Strict: Same Primitives, Different Policies

The `affine_awaitable` concept enables two distinct usage patterns with the same primitives:

| Approach | Policy | Use Case |
|----------|--------|----------|
| **Tiered** | Accept all, optimize what we can | Library adoption, gradual migration |
| **Strict** | Reject non-affine at compile time | Performance-critical systems, embedded |

Both approaches use the same `affine_awaitable` concept and protocol—the difference is in the policy enforced by `await_transform`:

**Tiered mode** uses `if constexpr` to detect affine awaitables and fall back to `make_affine` for non-affine ones:

```cpp
template<typename Awaitable>
auto await_transform(Awaitable&& a) {
    using A = std::remove_cvref_t<Awaitable>;
    if constexpr (affine_awaitable<A, Dispatcher>) {
        return affine_awaiter{std::forward<Awaitable>(a), &dispatcher_};
    } else {
        return make_affine(std::forward<Awaitable>(a), dispatcher_);
    }
}
```

**Strict mode** uses `static_assert` to reject non-affine awaitables at compile time:

```cpp
template<affine_awaitable<Dispatcher> A>
auto await_transform(A&& a) {
    return affine_awaiter{std::forward<A>(a), &dispatcher_};
}

template<typename A>
auto await_transform(A&& a) {
    static_assert(affine_awaitable<std::remove_cvref_t<A>, Dispatcher>,
                  "Only affine awaitables are accepted in strict mode");
    return affine_awaiter{std::forward<A>(a), &dispatcher_};
}
```

#### 6.1.3 Gradual Library Evolution

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

#### 6.1.4 Example: Strict-Mode Task

A strict-mode task rejects non-affine awaitables at compile time:

```cpp
template<typename T>
class strict_task {
    struct promise_type {
        any_dispatcher dispatcher_;
        
        template<class Awaitable>
        auto await_transform(Awaitable&& a) {
            using A = std::decay_t<Awaitable>;
            static_assert(affine_awaitable<A, any_dispatcher>,
                          "strict_task only accepts affine awaitables");
            return transform_awaiter<Awaitable>{
                std::forward<Awaitable>(a), this};
        }
        
        // ... rest of promise implementation
    };
    
    // ... rest of task implementation
};
```

This ensures that any attempt to `co_await` a non-affine awaitable results in a compile-time error, providing a hard guarantee of zero allocations.

#### 6.1.5 Migration Tool, Not Just Detection

The concept serves a dual purpose that should be explicitly recognized:

**Detection (tiered mode)**: `if constexpr` selection
- Detects whether an awaitable is affine at compile time
- Falls back to `make_affine` for non-affine awaitables
- Enables gradual migration and library compatibility

**Enforcement (strict mode)**: `static_assert` guarantee
- Rejects non-affine awaitables at compile time
- Provides hard zero-allocation guarantee
- Enables performance-critical and embedded use cases

The same `affine_awaitable` concept enables both patterns—it's not just about detection, but about providing a migration path from today's ecosystem to zero-overhead scheduler affinity.

---

## 7. Proposed Wording

*Relative to N4981.*

### 7.1 Header `<coroutine>` synopsis

Add to the `<coroutine>` header synopsis:

```cpp
namespace std {
  // [coroutine.affine.dispatcher], dispatcher concept
  template<class D, class P = void>
    concept dispatcher = see-below;

  // [coroutine.affine.concept], affine awaitable concept
  template<class A, class D, class P = void>
    concept affine_awaitable = see-below;
}
```

### 7.2 Dispatcher concept [coroutine.affine.dispatcher]

```cpp
template<class D, class P = void>
concept dispatcher = requires(D const& d, coroutine_handle<P> h) {
  { d(h) } -> std::convertible_to<std::coroutine_handle<>>;
};
```

*Remarks:* The dispatcher must return a `std::coroutine_handle<>` (or convertible type) to enable symmetric transfer. Calling `d(h)` schedules `h` for resumption (typically on a specific execution context) and returns a coroutine handle that the caller may use for symmetric transfer. The dispatcher must be const-callable (logical constness), enabling thread-safe concurrent dispatch from multiple coroutines. Since `coroutine_handle<P>` has `operator()` which invokes `resume()`, the handle itself is callable and can be dispatched directly.

### 7.3 Affine awaitable concept [coroutine.affine.concept]

```cpp
template<class A, class D, class P = void>
concept affine_awaitable =
  dispatcher<D, P> &&
  requires(A a, coroutine_handle<P> h, D const& d) {
    a.await_suspend(h, d);
  };
```

*Remarks:* This concept detects awaitables that participate in the affine awaitable protocol (§4) by providing the extended `await_suspend(h, d)` overload. The dispatcher is passed by const lvalue reference, and the awaitable must use it to resume the caller. The dispatcher returns a coroutine handle that `await_suspend` may return for symmetric transfer.

---

## 8. Summary

This proposal standardizes a minimal protocol for zero-overhead scheduler affinity in C++ coroutines:

| Component | Purpose |
|-----------|---------|
| `dispatcher<D,P>` | Concept for types callable with `coroutine_handle<P>` |
| `affine_awaitable<A,D,P>` | Concept detecting extended `await_suspend` protocol |

**The protocol (§4):** Awaitables that accept a dispatcher in `await_suspend(h, d)` can resume through it, achieving zero-overhead scheduler affinity. Task types propagate the dispatcher through `co_await` chains, ensuring every coroutine resumes where it belongs.

**The vision:** Every coroutine resumes where it belongs. No exceptions, no overhead, no rewrites.

---

## Acknowledgements

Thanks to Matheus Izvekov for feedback on reusable components and third-party coroutine extensibility. Thanks to Klemens Morgenstern for insights on real-world `await_transform` patterns and refining the dispatcher concept to accept coroutine handles directly. Thanks to Mateusz Pusz for detailed feedback on implementation patterns and P3552 integration. Thanks to the authors of P3552 (Dietmar Kühl, Maikel Nadolski) for establishing the foundation this proposal builds upon.

---

## References

### WG21 Papers

- **[P2300R10]** Michał Dominiak, Georgy Evtushenko, Lewis Baker, Lucian Radu Teodorescu, Lee Howes, Kirk Shoop, Michael Garland, Eric Niebler, Bryce Adelstein Lelbach. *`std::execution`*. [https://wg21.link/P2300R10](https://wg21.link/P2300R10)

- **[P3109R0]** Eric Niebler. *A plan for `std::execution` for C++26*. [https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p3109r0.html](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p3109r0.html)

- **[P3552R3]** Dietmar Kühl, Maikel Nadolski. *Add a Coroutine Task Type*. [https://wg21.link/P3552](https://wg21.link/P3552)

- **[P0981R0]** Gor Nishanov, Richard Smith. *Halo: Coroutine Heap Allocation eLision Optimization*. [https://wg21.link/P0981](https://wg21.link/P0981)

- **[P2855R1]** Ville Voutilainen. *Member customization points for Senders and Receivers*. [https://isocpp.org/files/papers/P2855R1.html](https://isocpp.org/files/papers/P2855R1.html)

- **[N4981]** Thomas Köppe. *Working Draft, Standard for Programming Language C++*. [https://wg21.link/N4981](https://wg21.link/N4981)

### Implementations

- **stdexec** — NVIDIA's reference implementation of P2300.  
  [https://github.com/NVIDIA/stdexec](https://github.com/NVIDIA/stdexec)

- **libunifex** — Meta's implementation of the Unified Executors proposal.  
  [https://github.com/facebookexperimental/libunifex](https://github.com/facebookexperimental/libunifex)

- **Intel Bare Metal Senders and Receivers** — Embedded adaptation demonstrating implementation challenges.  
  [https://github.com/intel/cpp-baremetal-senders-and-receivers](https://github.com/intel/cpp-baremetal-senders-and-receivers)

### Existing Awaitable Ecosystem

- **Boost.Asio** — Cross-platform C++ library for network and low-level I/O programming, with coroutine support since Boost 1.66 (2017).  
  [https://www.boost.org/doc/libs/release/doc/html/boost_asio.html](https://www.boost.org/doc/libs/release/doc/html/boost_asio.html)

- **cppcoro** — Lewis Baker's coroutine library providing task types, generators, and async primitives. Note: This library is not actively maintained.  
  [https://github.com/lewissbaker/cppcoro](https://github.com/lewissbaker/cppcoro)

- **libcoro** — Modern C++20 coroutine library.  
  [https://github.com/jbaldwin/libcoro](https://github.com/jbaldwin/libcoro)

- **Bloomberg Quantum** — Production multi-threaded coroutine dispatcher.  
  [https://github.com/bloomberg/quantum](https://github.com/bloomberg/quantum)

- **folly::coro** — Meta's coroutine library used in production at scale.  
  [https://github.com/facebook/folly/tree/main/folly/coro](https://github.com/facebook/folly/tree/main/folly/coro)

### Background Reading

- Corentin Jabot. *A Universal Async Abstraction for C++*. Discussion of executor models and design trade-offs.  
  [https://cor3ntin.github.io/posts/executors/](https://cor3ntin.github.io/posts/executors/)

- Lewis Baker. *C++ Coroutines: Understanding operator co_await*.  
  [https://lewissbaker.github.io/2017/11/17/understanding-operator-co-await](https://lewissbaker.github.io/2017/11/17/understanding-operator-co-await)

- Lewis Baker. *C++ Coroutines: Understanding the promise type*.  
  [https://lewissbaker.github.io/2018/09/05/understanding-the-promise-type](https://lewissbaker.github.io/2018/09/05/understanding-the-promise-type)

---

## Revision History

### R0 (2025-12-30)

Initial revision. This paper supersedes the earlier "Simplifying P3552R3 with `make_affine`" draft, which focused narrowly on the trampoline fallback. This revision presents a complete framework:

- Introduces the `affine_awaitable` concept for zero-overhead scheduler affinity
- Provides helpers to bridge standard coroutine machinery
- Positions affine awaitables as a foundation that P3552's `task` can build upon

The key insight: passing the dispatcher to the awaitable eliminates the trampoline allocation entirely for opt-in awaitables, while maintaining full compatibility with existing code.

### R1 (2025-12-31)

Incorporated feedback and clarifications contributed by Logan McDougall:
- Added formal protocol link and clarified adapter direction in §4.3
- Clarified tiered vs. strict policies and migration roles in §6
- Simplified allocation table and references; documented `done_flag_` and reference link
- Noted library implementability and dispatcher extensibility

### R2 (2025-12-31)

Focused proposal on core protocol and concepts:
- Removed implementation details (mixins, trampoline, tiered/strict modes) from normative sections
- Inserted protocol specification verbatim from `affine-awaitable-protocol.md` (§4)
- Merged concepts into protocol section (§4.1, §4.2)
- Added non-normative section (§5) describing implementation examples with clear statement that mixins are not proposed
- Simplified proposed wording to only include concepts
- Maintained emotional arc and technical tone while narrowing scope

### R3 (2026-01-03)

Refinements based on implementation and feedback:
- Updated dispatcher concept to require return of `std::coroutine_handle<>` for symmetric transfer (§4.1, §7.2)
- Updated all examples and proposed wording to reflect dispatcher return value requirement
- Added implementation patterns section (§6.1) describing tiered vs. strict modes, non-breaking overload addition, and gradual migration path
- Restructured section 6: converted main benefit headings to bold format, renumbered implementation patterns subsection
- Consolidated helper type lists to single canonical reference, replaced other mentions with generic "helpers"
- Updated reference implementation: made `make_affine.hpp` self-contained, removed duplicate definitions from `affine_helpers.hpp`

### R4 (2026-01-10)

Aligned paper with reference implementation:
- Updated §4.4 point 3: legacy `await_suspend(caller)` path is now optional; strict-mode tasks may omit it
- Simplified §5: removed mixin examples, now points directly to reference implementation at corosio
- Updated all repository links to https://github.com/cppalliance/corosio
- Updated §6.1.4 example to use actual implementation patterns (`transform_awaiter`, `any_dispatcher`)
- Removed references to mixins (`affine_promise`, `affine_task`, `affine_awaiter`) throughout


