# Type-Erased Executors: Performance Analysis

Analysis of the performance trade-offs when using a polymorphic `any_executor`
wrapper versus concrete template-based executors, based on N4242 and N4482.

## Introduction

N4242 proposes a two-layer executor design:

1. **Concrete executor types** — templates like `thread_pool::executor_type`,
   `strand<Executor>`, where the compiler sees the full implementation
2. **Polymorphic wrapper** — a type-erased `executor` class for runtime flexibility

The question arises: can we use only the type-erased wrapper and still get
the performance benefits of templates? The answer is no, and this document
explains why with concrete code examples.

## The Executor Implementations

```cpp
// Concrete executor - compiler sees everything
struct pool_executor {
    thread_pool* pool_;
    
    template<class F>
    void dispatch(F&& f) {
        if (pool_->running_in_this_thread())
            std::forward<F>(f)();  // inline execution
        else
            pool_->enqueue(std::forward<F>(f));
    }
};

// Type-erased wrapper (like std::function but for executors)
class executor {
    struct ops {
        void (*dispatch)(void*, void*);
    };
    ops const* ops_;
    void* target_;
    
public:
    template<class F>
    void dispatch(F&& f) {
        // Must wrap F into type-erased callable, then indirect call
        auto wrapper = [](void* p) { (*static_cast<F*>(p))(); };
        ops_->dispatch(target_, /*erased f*/);
    }
};
```

## Using Them in a Coroutine

```cpp
// TEMPLATE VERSION
template<class Executor>
struct final_awaiter {
    Executor ex;
    std::coroutine_handle<> parent;
    
    void await_suspend(std::coroutine_handle<>) {
        ex.dispatch([h = parent]{ h.resume(); });
    }
};

// TYPE-ERASED VERSION  
struct final_awaiter {
    executor ex;  // type-erased
    std::coroutine_handle<> parent;
    
    void await_suspend(std::coroutine_handle<>) {
        ex.dispatch([h = parent]{ h.resume(); });
    }
};
```

## What the Compiler Generates

**Template version** — after inlining, `await_suspend` becomes:

```cpp
void await_suspend(std::coroutine_handle<>) {
    // ex.dispatch(...) fully inlined:
    if (ex.pool_->running_in_this_thread()) {
        parent.resume();  // direct call
    } else {
        ex.pool_->enqueue([h = parent]{ h.resume(); });
    }
}
```

**Type-erased version** — compiler cannot inline past the function pointer:

```cpp
void await_suspend(std::coroutine_handle<>) {
    // ex.dispatch(...) emits:
    auto* fn_ptr = ex.ops_->dispatch;  // load from memory
    (*fn_ptr)(ex.target_, /*wrapped lambda*/);  // indirect call
    // ^^^ compiler has no idea what fn_ptr points to
    // ^^^ cannot inline, cannot optimize, cannot eliminate
}
```

## The Optimization That's Lost

From N4242 §9, the key insight for `defer()`:

```cpp
// Template: compiler sees this is a continuation, can use thread-local queue
template<class Executor>
void complete_read(Executor& ex) {
    ex.defer([this]{ read_loop(); });
    // Compiler inlines defer(), sees thread-local optimization,
    // emits: local_queue.push_back(handler) — no lock!
}

// Type-erased: compiler just sees an indirect call
void complete_read(executor& ex) {
    ex.defer([this]{ read_loop(); });
    // Compiler emits: call through function pointer
    // Cannot see the thread-local optimization exists
}
```

## strand<Executor> — The Real Cost

```cpp
// Fully optimized: both strand logic AND inner dispatch inline
strand<pool_executor> s(pool.get_executor());
s.dispatch([]{...});
// Compiler produces: check strand, check pool thread, call directly

// One level of indirection
strand<executor> s(type_erased);
s.dispatch([]{...});  
// Compiler: inlines strand logic, but inner ex.dispatch() is indirect call

// Wrapped strand — virtual at boundary
executor ex = strand<pool_executor>(pool.get_executor());
ex.dispatch([]{...});
// Compiler: indirect call to strand::dispatch, 
// but INSIDE that, the pool dispatch is inlined
```

## Performance Impact

From N4242 §9:

> "On recent hardware we can observe an uncontended lock/unlock cost of some
> 10 to 15 nanoseconds, compared with 1 to 2 nanoseconds for accessing a
> thread-local queue."

The `defer()` optimization saves ~13ns per call. But with type erasure, the
compiler can't see that `defer()` uses a thread-local queue, so it can't
eliminate the lock. The indirect call itself adds another ~2-5ns (branch
misprediction + icache miss).

**Per-coroutine-switch overhead**: ~15-20ns with type erasure vs ~2ns with templates.

At 100,000 switches/second: 2ms vs 0.2ms — 10x difference.

## Conclusion

Type erasure has its place (runtime polymorphism, ABI boundaries, heterogeneous
containers), but for hot paths like coroutine continuations, templates are
essential for zero-overhead abstraction.

The N4242 design provides both: use templates by default, type-erase only
when you need the flexibility.

## References

- N4242: Executors and Asynchronous Operations, Revision 1 (2014)
- N4482: Some notes on executors and the Networking Library Proposal (2015)
