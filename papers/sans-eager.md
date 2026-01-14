Great question. Let me map out the I/O patterns and see what actually requires eager semantics.

## Pattern Analysis

### Pattern A: Fire and Forget (Server Handlers)
```cpp
// Eager:
eager_on(ex, handle_connection(sock));

// Lazy equivalent:
async_run(ex, handle_connection(sock));
```
**Verdict: ✓ Lazy works.** `async_run` is fire-and-forget by design.

---

### Pattern B: Concurrent Fan-out, Gather Results
```cpp
// Eager:
auto a = eager_on(ex, fetch("/users"));
auto b = eager_on(ex, fetch("/posts"));  // both running now
auto users = co_await a;
auto posts = co_await b;

// Lazy equivalent:
auto [users, posts] = co_await when_all(
    fetch("/users"),
    fetch("/posts")
);
```
**Verdict: ✓ Lazy works** with a `when_all` combinator.

---

### Pattern C: Dynamic Fan-out (Unknown N)
```cpp
// Eager:
vector<eager_task<Result>> tasks;
for (auto& url : urls)
    tasks.push_back(eager_on(ex, fetch(url)));
// all running concurrently
for (auto& t : tasks)
    results.push_back(co_await t);

// Lazy equivalent:
vector<task<Result>> tasks;
for (auto& url : urls)
    tasks.push_back(fetch(url));
auto results = co_await when_all(std::move(tasks));
```
**Verdict: ✓ Lazy works** with range-accepting `when_all`.

---

### Pattern D: Timeout / Racing
```cpp
// Eager:
auto op = eager_on(ex, slow_operation());
auto timer = eager_on(ex, sleep(5s));
// whichever completes first...

// Lazy equivalent:
auto result = co_await when_any(
    slow_operation(),
    sleep(5s)
);
```
**Verdict: ✓ Lazy works** with a `when_any` combinator.

---

### Pattern E: Managed Connection Pool (Klemens' Example)
```cpp
// Eager - full control:
vector<eager_task<void>> connections;
while (accepting) {
    auto sock = co_await listener.accept();
    connections.push_back(eager_on(ex, handle(sock)));
}
// Later: can iterate, cancel specific ones, await all for graceful shutdown
for (auto& c : connections)
    c.request_stop();
for (auto& c : connections)
    co_await c;

// Lazy approximation:
std::atomic<int> active_count{0};
stop_source shutdown;
while (accepting) {
    auto sock = co_await listener.accept();
    ++active_count;
    async_run(ex, [&]() -> task<> {
        co_await handle(sock, shutdown.get_token());
        --active_count;
    }());
}
// Graceful shutdown:
shutdown.request_stop();
while (active_count > 0)
    co_await sleep(10ms);  // poll (ugly)
```
**Verdict: ⚠️ Lazy works but loses elegance.** No direct handle to individual tasks. Shutdown requires external coordination.

---

### Pattern F: Start Now, Await Later (True Interleaving)
```cpp
// Eager - true concurrency within one coroutine:
auto computation = eager_on(cpu_ex, heavy_compute());
co_await do_io_stuff();           // computation running in parallel
co_await do_more_io();            // still running
auto result = co_await computation; // get result

// Lazy - no equivalent:
// Can't interleave a single long operation with other awaits
// Would need to restructure as when_all or use external future
```
**Verdict: ✗ No clean lazy equivalent.** This is the one pattern that truly requires eager semantics or an external future type.

---

## Summary Table

| Pattern | Eager Needed? | Lazy Alternative |
|---------|---------------|------------------|
| Fire-and-forget handlers | No | `async_run` |
| Fixed concurrent fan-out | No | `when_all` |
| Dynamic concurrent fan-out | No | `when_all(range)` |
| Timeout / race | No | `when_any` |
| Managed task collection | Partial | External tracking + stop_token |
| Start-now-await-later | **Yes** | External future or restructure |

## What You Actually Lose Without Eager

1. **Elegant task management** - Tracking/cancelling individual tasks in a dynamic set
2. **True interleaved concurrency** - Running one thing while awaiting others in the same coroutine
3. **Compositional concurrency** - Building complex concurrent patterns without restructuring code

## What Combinators Buy You

With just `when_all` and `when_any`, you cover ~90% of real I/O use cases:

```cpp
// Parallel requests with timeout:
auto [result, _] = co_await when_any(
    when_all(fetch(url1), fetch(url2), fetch(url3)),
    sleep(30s)
);
```

## Recommendation

For a **minimal I/O library**, lazy tasks + combinators + `async_run` is sufficient. The remaining gap (Pattern F) is niche and can be filled by users bringing their own future type if needed.

Eager tasks are a "task library" concern, not an "I/O library" concern. The I/O primitives don't care whether the task awaiting them is eager or lazy.