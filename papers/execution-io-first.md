| Document | D0000 |
|----------|-------|
| Date:       | 2025-01-05
| Reply-to:   | Vinnie Falco \<vinnie.falco@gmail.com\>
| Audience:   | SG1, LEWG

---

# A Coroutines-First I/O Execution Model

## Abstract

This paper asks: *what would an execution model look like if designed from the ground up for coroutine-driven asynchronous I/O?*

An execution model answers how asynchronous work is scheduled and dispatched, where operation state is allocated and by whom, and what customization points allow users to adapt the framework's behavior. We propose a **coroutines-first** execution model optimized for CPU-bound I/O workloads. The framework comprises: a minimal executor abstraction built on `dispatch` and `post`; the **affine awaitable protocol** for zero-overhead scheduler affinity; dispatcher propagation through coroutine chains; and allocator customization via I/O object arguments.

Central to our design is a clear responsibility model: **the executor decides allocation policy**, while **the I/O object owns its executor**. The I/O object becomes a unified carrier—owning the dispatcher binding and carrying the allocator reference through composed operation chains. Both resources propagate forward through I/O object arguments, addressing timing constraints that backward query models cannot satisfy. The result is a simpler abstraction: two operations, clear ownership semantics, forward context propagation, and direct mapping to what I/O completion paths actually do.

An explicit tradeoff: we consciously trade one pointer indirection per I/O operation (~1-2 nanoseconds) for complete type hiding. Executor types do not leak into public interfaces; platform I/O types remain hidden in translation units; composed algorithms expose only `task` return types. This type hiding directly enables ABI stability—library implementations can evolve without breaking user binaries. It also eliminates the template complexity and compile time bloat that have long plagued C++ async libraries. In contrast, `std::execution`'s `connect(sender, receiver)` returns a fully-typed `operation_state`, forcing implementation types through every API boundary.

---

## 1. Introduction

The C++ standardization effort has produced `std::execution` (P2300), a sender/receiver framework for asynchronous programming. Its design accommodates heterogeneous computing—GPU kernels, parallel algorithms, and hardware accelerators. The machinery is substantial: domains select algorithm implementations, queries determine completion schedulers, and transforms dispatch work to appropriate backends.

But what does asynchronous I/O actually need?

A network server completes read operations on I/O threads, resumes handlers on application threads, and serializes access to connection state. A file system service batches completions through io_uring, dispatches callbacks to worker pools, and manages buffer lifetimes across suspension points. These patterns repeat across every I/O-intensive application. None of them require domain-based algorithm dispatch. TCP servers do not run on GPUs. Socket reads have one implementation per platform, not a menu of hardware backends.

**This paper explores what emerges when I/O requirements drive the design.**

We find that two operations suffice: **dispatch** for continuations and **post** for independent work. The choice between them is correctness, not performance. Using dispatch while holding a mutex invites deadlock. Using post for every continuation wastes cycles bouncing through queues. But primitives alone don't solve the composition problem. In a chain like `async_http_request → async_read → socket`, who decides which allocator to use? Who determines which executor runs the completion? The answers are not symmetric:

- **The executor decides allocation policy.** Memory strategy—recycling pools, bounded allocation, per-tenant budgets—is a property of the execution context, not the I/O operation. A socket doesn't care about memory policy; the context in which it runs does.

- **The I/O object owns its executor.** A socket registered with epoll on thread A cannot have completions delivered to thread B's epoll. The physical coupling is inherent. The call site cannot know which event loop the socket uses—only the socket knows.

This responsibility model makes the I/O object a **unified carrier**: it owns the executor binding and carries the allocator reference (provided by the executor at construction). Every coroutine in the chain receives both resources through the I/O object argument. The timing constraints matter. Frame allocation occurs before the coroutine body executes—the allocator must be discoverable from arguments, not injected via `await_transform`. The dispatcher must be active from the first instruction—code before the first `co_await` must run in the correct context, not just code after suspension.

We show that execution context flows naturally *forward* through async operation chains, eliminating the backward query model that `std::execution` struggles to fix. The result is a simpler abstraction with clear ownership semantics. Our goal is not to replace `std::execution` for GPU workloads. Our goal is to demonstrate that networking deserves first-class design consideration—a purpose-built abstraction rather than adaptation to a framework optimized for different requirements.

---

## 2. Motivation

### 2.1 Why Not the Networking TS?

The Networking TS (and its progenitor Boost.Asio) is the de facto standard for asynchronous I/O in C++. It is mature, well-tested, and supports coroutines through completion tokens. Why build something new?

#### 2.1.1 The Template Tax

The Networking TS design philosophy—zero-overhead abstraction through templates—incurs costs that compound in large codebases:

**Every async operation signature includes executor and handler types:**
```cpp
template<class Executor, class ReadHandler>
void async_read(basic_socket<Protocol, Executor>& s,
                MutableBufferSequence const& buffers,
                ReadHandler&& handler);
```

**Composed operations propagate these types:**
```cpp
template<class Executor, class Handler>
void async_http_request(basic_socket<tcp, Executor>& sock,
                        http::request const& req,
                        Handler&& handler);
```

This creates:
- **N×M template instantiations** for N operations × M executor/handler combinations
- **Header-only dependencies** that must be recompiled on change
- **Binary size growth** that can reach megabytes in complex applications
- **Compile times measured in minutes** for moderate codebases
- **Nested move-construction overhead** at runtime—composed handlers like `read_op<request_op<session_op<lambda>>>` must be moved at each abstraction layer, with costs that compound as nesting depth increases

#### 2.1.2 The Encapsulation Problem

The Networking TS templates expose implementation details through public interfaces:

```cpp
// User sees:
class http_client
{
public:
    template<class Executor, class Handler>
    void async_get(Executor ex, std::string url, Handler&& h);
};
```

The user is forced to know:
- What executor types are valid
- What handler signatures are expected
- That the implementation uses sockets at all

Platform types (`OVERLAPPED`, `io_uring_sqe`) do not appear in public API signatures—users write portable code. But they are not hidden from translation units. Asio is header-only; its internal operation types publicly inherit from `OVERLAPPED`; including `<boost/asio.hpp>` pulls Windows headers into every compilation unit. The *API* is portable; the *compilation* is not. And the *structure* of the async machinery still leaks through every API boundary.

**Our approach:**
```cpp
class http_client
{
public:
    task async_get(std::string url);  // That's it.
};
```

The implementation—sockets, buffers, executors—lives in the translation unit. The interface is stable. The ABI is stable. Compilation is fast.

#### 2.1.3 Coroutine-Compatible vs Coroutine-First

The Networking TS added coroutine support through completion tokens like `use_awaitable`:

```cpp
co_await async_read(socket, buffer, use_awaitable);
```

This adapts callback-based operations for coroutines. It works, but:
- **Double indirection**: Callback machinery wraps coroutine machinery
- **Executor handling is manual**: `co_spawn` and `bind_executor` required
- **Error handling diverges**: Exceptions vs `error_code` vs `expected`
- **Mental model mismatch**: Writing coroutines that think in callbacks

Our design is **coroutine-first**: the suspension/resumption model is the foundation, not an adapter. Executor propagation is automatic. Type erasure is structural. The callback path (`dispatch().resume()`) is the compatibility layer, not the other way around.

#### 2.1.4 When to Use the Networking TS Instead

The Networking TS remains the right choice when:
- You need callback-based APIs for C compatibility
- Template instantiation cost is acceptable
- You're already invested in the Asio ecosystem
- Maximum performance with zero abstraction is required
- Standardization timeline matters for your project

Our framework is better suited when:
- Coroutines are the primary programming model
- Public APIs must hide implementation details
- Compile time and binary size matter
- ABI stability is required across library boundaries
- Clean, simple interfaces are prioritized

### 2.2 Why Not std::execution?

The C++26 `std::execution` framework (P2300) provides sender/receiver abstractions for asynchronous programming. We analyzed both the base proposal and its evolution to understand how well it addresses networking.

#### 2.2.1 P2300: Networking Mentioned, Not Addressed

P2300 references networking and GPU/parallel topics in roughly equal measure. Superficially balanced. However, the *nature* of these references differs:

| Aspect | GPU/Parallel | Networking |
|--------|--------------|------------|
| Concrete primitives | `bulk` algorithm, parallel scheduler | None — deferred to P2762 |
| Platform integration | CUDA mentioned by name | No IOCP, epoll, or io_uring |
| Serialization | N/A | **Zero strand mentions** |
| Status | Normative specification | Illustrative examples only |

P2300 explicitly defers networking:

> "Dietmar Kuehl has proposed networking APIs that use the sender/receiver abstraction (see P2762)." — P2300, Section 1.4.1.3

The framework provides composition primitives but **no I/O operations, no buffer management, and no strand serialization**. Networking exists as examples, not as design drivers.

**What P2762 actually proposes:**

P2762, the proposal P2300 defers to, explicitly chooses the backward query model as "most useful":

> "Injecting the used scheduler from the point where the asynchronous work is actually used."

Yet the paper immediately acknowledges the late-binding problem:

> "When the scheduler is injected through the receiver the operation and used scheduler are brought together rather late, i.e., when `connect(sender, receiver)`ing and when the work graph is already built."

P2762 also raises per-operation cancellation overhead as a concern—the same issue our socket-level approach solves—but offers no solution:

> "Repeatedly doing that operation while processing data on a socket may be a performance concern... This area still needs some experimentation and, I think, design."

P2762 leaves core networking features unspecified:

| Feature | P2762 Status |
|---------|--------------|
| Scheduler interface | "It isn't yet clear how such an interface would actually look" |
| Strand serialization | Not mentioned |
| Buffer pools | "There is currently no design" |

This confirms our thesis: networking is being adapted to `std::execution` rather than driving its design. The proposal acknowledges problems with the query model, identifies performance concerns we solve, yet leaves core networking abstractions unaddressed.

#### 2.2.2 P3826: GPU-First Evolution

P3826R2, "Fix or Remove Sender Algorithm Customization," reveals where `std::execution`'s complexity originates. The paper identifies a fundamental flaw:

> "Many senders do not know where they will complete until they know where they will be started."

The proposed fix adds `get_completion_domain<Tag>` queries, `indeterminate_domain<>` types, and double-transform dispatching. This machinery exists to select GPU vs CPU algorithm implementations.

**P3826 contains over 100 references to GPU, parallel execution, and hardware accelerators. It contains zero references to networking, sockets, or I/O patterns.**

| Concern | GPU/Parallel (P3826's Focus) | Networking (Unaddressed) |
|---------|------------------------------|--------------------------|
| Algorithm dispatch | Critical — GPU vs CPU kernel | Irrelevant — one implementation per platform |
| Completion location | May differ from start | Same context or OS completion port |
| Domain customization | Required for acceleration | Unused overhead |

Networking requires different things entirely:
- **Strand serialization**: Ordering guarantees, not algorithm selection
- **I/O completion contexts**: IOCP, epoll, io_uring integration
- **Executor propagation**: Ensuring handlers run on the correct thread
- **Buffer lifetime management**: Zero-copy patterns, scatter/gather

None appear in P3826's analysis.

#### 2.2.3 The Query Model Problem

Both P2300 and P3826 share a design choice: **backward queries** for execution context.

```cpp
// std::execution pattern: query sender/receiver for properties
auto sched = get_scheduler(get_env(rcvr));
auto domain = get_domain(get_env(sndr));

// P3826 addition: tell sender where it starts
auto domain = get_completion_domain<set_value_t>(get_env(sndr), get_env(rcvr));
```

This requires senders to be self-describing—problematic when context is only known at the call site. P3826 attempts to fix this by passing hints, but the fix adds complexity rather than questioning the query model itself.

#### 2.2.4 Forward Propagation Alternative

Our coroutine model propagates context forward:

```cpp
// Coroutine-first: context flows from caller to callee
async_run(my_executor, my_task());  // Executor injected at root
// await_transform propagates any_executor const& to all children — no queries needed
```

The executor is always known because it flows with control flow, not against it. No domain queries. No completion scheduler inference. No transform dispatching.

#### 2.2.5 Observations

Comparing our networking-first design with `std::execution` reveals significant divergence:

1. **What we need, they don't have**: Strand serialization, platform I/O integration, buffer management
2. **What they have, we don't need**: Domain-based algorithm dispatch, completion domain queries, sender transforms
3. **Context flow**: They query backward; we inject forward

The `std::execution` framework may eventually support networking, but its evolution is driven by GPU and parallel workloads. The complexity P3826 adds—to fix problems networking doesn't have—suggests that networking was not a primary design consideration.

A framework designed from networking requirements produces a simpler, more direct solution.

#### 2.2.6 The std::execution Tax

If networking is required to integrate with `std::execution`, I/O libraries must pay a complexity tax:

- **Query protocol compliance**: Implement `get_env`, `get_domain`, `get_completion_scheduler`—even if only to return defaults
- **Concept satisfaction**: Meet sender/receiver requirements designed for GPU algorithm dispatch
- **Transform machinery**: Domain transforms execute even when they select the only available implementation
- **API surface expansion**: Expose attributes and queries irrelevant to I/O operations
- **Type leakage**: `connect(sender, receiver)` returns a fully-typed `operation_state`, forcing implementation details through every API boundary

The type leakage deserves emphasis. In the sender/receiver model:

```cpp
// std::execution pattern
execution::sender auto snd = socket.async_read(buf);
execution::receiver auto rcv = /* ... */;
auto state = execution::connect(snd, rcv);  // Type: connect_result_t<Sender, Receiver>
```

The `connect_result_t` type encodes the full operation state. Algorithms that compose senders must propagate these types:

```cpp
// From P2300: operation state types leak into composed operations
template<class S, class R>
struct _retry_op {
    using _child_op_t = stdexec::connect_result_t<S&, _retry_receiver<S, R>>;
    optional<_child_op_t> o_;  // Nested operation state, fully typed
};
```

This tax exists regardless of whether networking benefits from it. A socket that returns `default_domain` still participates in the dispatch protocol. The P3826 machinery runs, finds no customization, and falls through to the default—overhead for nothing.

**The question is not whether P2300/P3826 break networking code.** They don't—defaults work. **The question is whether networking should pay for abstractions it doesn't use.** Our analysis suggests the cost is not justified when a simpler, networking-native design achieves the same goals.

#### 2.2.7 Divergent Standardization Efforts

WG21 faces a troubling pattern: std::execution continues evolving without addressing networking needs (§2.2.1-2.2.6), while separate proposals ([P3185](https://wg21.link/p3185), [P3482](https://wg21.link/p3482)) advocate replacing proven socket-based designs with the IETF TAPS framework—an abstract architecture with essentially no production deployment after a decade of development.

Our analysis in [IETF TAPS versus Boost.Asio](https://github.com/cppalliance/wg21-papers/blob/master/ietf-taps-vs-asio.md) documents the risks of framework-first design. History favors proven practice: TCP/IP over OSI, BSD sockets over every proposed replacement, Boost.Asio's twenty years of field experience over speculative architecture.

This paper demonstrates that a networking-first coroutine framework is both achievable and practical—without waiting for std::execution to address needs it wasn't designed for, and without adopting an untested abstraction model.

### 2.3 Why Not Wait For P3482 (IETF TAPS)?

P3185 and P3482 propose aligning C++ networking with the IETF Transport Services (TAPS) initiative—an abstract architecture where applications specify properties (reliability, ordering, latency) and the transport system selects protocols at runtime. The vision is appealing: protocol agility, connection racing, secure-by-default semantics. The reality is sobering. After a decade of IETF standardization work, only one production implementation exists: Apple's proprietary Network.framework, which predates TAPS and is not fully compliant. The open-source NEAT implementation, funded by the EU's Horizon 2020 program, was abandoned immediately when funding ended in 2018. No sustained community emerged; no living tradition of knowledge survives.

This pattern mirrors OSI's failure against TCP/IP. Framework-first designs—comprehensive, theoretically elegant, developed through specification processes—consistently lose to pragmatic, use-case-driven designs refined through deployment. BSD sockets, despite their warts, achieved universal adoption because they solved real problems programmers actually had. Asio built on this foundation with twenty years of continuous evolution: C++11 move semantics, executor model refinement, C++20 coroutine support. Each adaptation responded to production feedback, not committee speculation. The burden of proof lies with the replacement: TAPS has accumulated draft revisions while Asio has accumulated deployments. History's verdict on framework-first versus practice-first is unambiguous.

---

## 3. Different Workloads, Different Needs

### 3.1 What GPU Workloads Need

GPU and parallel workloads require **algorithm dispatch**—selecting the right implementation for the hardware. A matrix multiply may run on CPU, GPU, or specialized accelerator. The framework must:

- **Query completion domains**: Where will this work finish? (P3826's central concern)
- **Select algorithm implementations**: `bulk` on GPU vs CPU has different code paths
- **Manage hardware heterogeneity**: CUDA, SYCL, OpenMP backends

P3826 adds `get_completion_domain<Tag>` queries and double-transform dispatching precisely for this: the sender doesn't know where it runs until connected to a receiver that reveals the target hardware.

### 3.2 What I/O Workloads Need

I/O workloads have fundamentally different requirements. A socket read has **one implementation per platform**—there's no GPU vs CPU choice. Instead, I/O needs:

- **Completion contexts**: Integration with IOCP, epoll, io_uring
- **Strand serialization**: Ordering guarantees for concurrent access
- **Buffer lifetime management**: Zero-copy patterns, scatter/gather
- **Thread affinity**: Handlers must resume on the correct thread

None of these appear in P3826's analysis. The framework's complexity serves algorithm selection—a problem I/O doesn't have.

### 3.3 The Executor is Fundamental

Both workloads share one need: **something that runs work**. Strip away algorithm dispatch, domain queries, and hardware selection. What remains?

Two operations:
- **`dispatch`**: Run this continuation (inline if safe)
- **`post`**: Queue this independent work (never inline)

The choice between them is **correctness, not optimization**. Using `dispatch` while holding a mutex invites deadlock. Using `post` for every continuation wastes cycles bouncing through queues.

The **executor**—a type providing `dispatch` and `post`—is the minimal foundation. Everything else (senders, schedulers, algorithm dispatch) can be built on top. For I/O workloads, nothing else is needed.

### 3.4 Allocation Control is Essential

Neither callbacks nor coroutines achieve competitive performance without allocation control. Early testing showed that fully type-erased callbacks—using `std::function` at every composition boundary—allocated hundreds of times per invocation and ran an order of magnitude slower than native templates. Non-recycling coroutine configurations perform so poorly, dominated by heap allocation overhead, that they are not worth benchmarking.

**Recycling is mandatory for performance.** Thread-local recycling—caching recently freed memory for immediate reuse—enables zero steady-state allocations. Callbacks achieve this through "delete before dispatch": deallocate operation state before invoking the completion handler. Coroutines achieve it through frame pooling: custom `operator new/delete` recycles frames across operations.

But thread-local recycling optimizes for throughput, not all applications. Different deployments have different needs:

- **Memory conservation**: Embedded systems or resource-constrained environments may prefer bounded pools over unbounded caches
- **Per-tenant limits**: Multi-tenant servers need allocation budgets per client to prevent resource exhaustion
- **Debugging and profiling**: Development builds may want allocation tracking that production builds eliminate

Coroutines are continuously created—every `co_await` may spawn new frames. An execution model must make allocation a first-class customization point that controls *all* coroutine allocations, not just selected operations. Without this, memory policy becomes impossible to enforce.

---

## 4. Who Owns What?

### 4.1 The Problem: Propagation Through Chains

Consider a composed coroutine chain:

```
http_client → http_request → write → write_some → socket
```

Each coroutine in this chain needs two resources:

1. **An allocator** for its frame. Where does memory come from?
2. **A dispatcher** for its completion. Where does control flow resume?

The questions seem symmetric but have different answers:

- Should every frame in the chain use the **same allocator**? Usually yes—the allocation strategy is a deployment decision, not an operation-specific choice.
- Should every step use the **same dispatcher**? Usually yes—the completion context is determined by the outermost caller, not by intermediate operations.

The challenge is *how* these resources reach each coroutine. In `std::execution`, context flows backward through queries:

```cpp
// std::execution pattern: query receiver for scheduler
auto sched = get_scheduler(get_env(receiver));
```

P2762 acknowledges the timing problem this creates:

> "When the scheduler is injected through the receiver the operation and used scheduler are brought together rather late, i.e., when `connect(sender, receiver)`ing and when the work graph is already built."

By the time the sender knows its scheduler, the operation structure is fixed. Allocation decisions that should have been made at construction time are deferred until connection time—too late for coroutine frame allocation, which occurs before the coroutine body executes.

We propose a different model: **forward propagation**. The executor and allocator are determined at I/O object construction and flow forward through the call chain. Every coroutine receives both resources through its I/O object argument, available from the first instruction.

### 4.2 The Executor Decides Allocation Policy

Memory strategy is a property of the **execution context**, not the I/O operation. A socket doesn't care whether its coroutine frames are pooled, bounded, or tracked—it performs the same read regardless. But the *context* in which that socket runs cares deeply:

**High-throughput server.** Thread-local recycling pools achieve zero steady-state allocations. The executor maintains per-thread free lists; coroutine frames are recycled immediately upon completion. This is the default for production I/O contexts.

**Real-time audio.** Bounded pools prevent allocation latency spikes. The executor pre-allocates a fixed pool at startup; operations that exceed the pool fail fast rather than blocking on the system allocator. Predictable latency matters more than handling arbitrary load.

**Multi-tenant isolation.** Per-tenant budgets prevent resource exhaustion. The executor tracks allocations per client; operations that exceed a tenant's quota are rejected before they can starve other tenants. Memory policy becomes a security boundary.

**Debugging and profiling.** Development builds may want allocation tracking—counting frames, detecting leaks, measuring pool efficiency. Production builds eliminate this overhead entirely.

The executor is the natural owner of allocation policy because:

1. **The executor knows the deployment context.** A server process knows its concurrency model, memory constraints, and tenant boundaries. Individual I/O operations do not.

2. **The executor outlives individual operations.** Recycling requires memory that persists across operations. The executor's lifetime spans the entire I/O workload.

3. **The executor can provide the allocator at construction time.** When an I/O object is created, the executor supplies an allocator reference. This reference propagates forward through every coroutine that uses the I/O object.

The result: memory policy is configured once at executor construction, and every operation inherits it automatically.

### 4.3 The I/O Object Owns Its Executor

Unlike allocation policy, executor ownership cannot be separated from the I/O object. The coupling is physical:

**Platform I/O is context-specific.** A socket registered with epoll on thread A cannot have completions delivered to thread B's epoll instance. An IOCP handle belongs to a specific completion port. An io_uring submission queue is tied to a specific ring. The I/O object *must* know its completion context—it cannot be injected later.

P2762 acknowledges this reality in §4.1:

> "It was pointed out networking objects like sockets are specific to a context: that is necessary when using I/O Completion Ports or a TLS library and yields advantages when using, e.g., io_uring(2)."

**Safety requires affinity.** Delivering a completion to the wrong context is not merely inefficient—it is undefined behavior. The kernel expects the completion to be consumed by the registered event loop. Violating this expectation corrupts internal state.

**The call site cannot know.** When `async_http_request` calls `async_read`, it passes a socket reference. The HTTP layer doesn't know—and shouldn't need to know—which event loop the socket uses. That knowledge is encapsulated in the socket itself.

**Composition must be transparent.** If intermediate operations had to thread executor information through their signatures, every composed operation would need an executor parameter:

```cpp
// What we want to avoid:
template<class Executor>
task async_http_request(Executor ex, socket& s, request const& req);
```

Instead, the socket carries its executor internally. Composed operations pass the socket; the executor comes along for free:

```cpp
// What we achieve:
task async_http_request(socket& s, request const& req);
```

The executor binding happens once, at socket construction. From that point forward, every operation on the socket inherits the correct completion context without explicit parameter passing.

### 4.4 The I/O Object as Unified Carrier

The preceding sections establish two ownership rules:

1. The **executor** decides allocation policy (§4.2)
2. The **I/O object** owns its executor (§4.3)

Combining these rules, the I/O object becomes a **unified carrier**: it owns the executor binding and carries the allocator reference that the executor provides at construction time.

```
┌──────────────────────────────────────────────────────────────────┐
│                         Executor                                 │
│  • Decides allocation policy (pools, limits, tracking)           │
│  • Provides allocator reference at I/O object construction       │
│  • Defines completion context (event loop, strand)               │
└──────────────────────────────────────────────────────────────────┘
                              │
                              │ constructs with allocator + context
                              ▼
┌──────────────────────────────────────────────────────────────────┐
│                        I/O Object                                │
│  • Owns executor/dispatcher binding                              │
│  • Carries allocator reference from executor                     │
│  • Passed as argument to every async operation                   │
└──────────────────────────────────────────────────────────────────┘
                              │
                              │ argument to async operations
                              ▼
┌──────────────────────────────────────────────────────────────────┐
│                        Coroutines                                │
│  • Detect allocator via io_object concept                        │
│  • Receive dispatcher via await_transform injection              │
│  • Both resources available from the first instruction           │
└──────────────────────────────────────────────────────────────────┘
```

Every coroutine in the chain receives both resources through the I/O object argument:

- **Allocator**: The coroutine's `promise_type::operator new` detects `io_object` on the I/O object parameter and uses the executor's allocator for frame allocation.

- **Dispatcher**: The promise's `await_transform` extracts the dispatcher from the I/O object and injects it into every `co_await`, ensuring completions resume in the correct context.

This forward propagation model addresses timing constraints that backward query models cannot satisfy:

- **Allocation occurs before the coroutine body executes.** The allocator must be discoverable from arguments at `operator new` time—it cannot wait for `await_transform` injection or receiver connection.

- **The dispatcher must be active from the first instruction.** Code before the first `co_await` must run in the correct context. Strands, for example, require serialization from the very beginning, not just after the first suspension.

The I/O object is the natural carrier because it is always present: every I/O operation takes an I/O object as an argument. No additional parameters are needed. No queries are required. Context flows forward with the data.

---

## 5. Timing Constraints

### 5.1 Allocator: Before the Body Executes

Coroutine frame allocation has a fundamental timing constraint: **`operator new` executes before the coroutine body**. When a coroutine function is called, the compiler-generated code allocates the frame first, then begins execution at the initial suspend point. This sequence is fixed by the language.

```cpp
auto t = my_coro(sock);  // operator new called HERE
co_await t;              // await_transform kicks in HERE (too late for allocation)
```

The allocator must be discoverable at the moment of invocation—from information available in the function call itself. Any mechanism that injects context later (receiver connection, `await_transform`, explicit method calls) arrives too late to influence frame allocation.

C++ provides exactly one hook at the right time: **`promise_type::operator new`**. The compiler passes coroutine arguments directly to this overload, allowing the promise to inspect parameters and select an allocator:

```cpp
template<io_object Arg0, typename... Args>
static void* operator new(std::size_t size, Arg0& arg0, Args&...) {
    return arg0.get_executor().allocate_frame(size);
}
```

The `io_object` concept detects types that provide an executor via `get_executor()`. When the first or second argument satisfies this concept, the promise uses the executor's allocator. Otherwise, a global pool provides the default.

**Contrast with `std::execution`.** P2762 acknowledges that the receiver-based model brings scheduler and operation together "rather late"—at `connect()` time, after the work graph is already built. For coroutines, this timing is fundamentally incompatible with frame allocation. The allocator cannot wait for receiver queries; it must be present in the arguments.

### 5.2 Dispatcher: From the First Instruction

The dispatcher constraint is subtler than allocation timing. Within a composed coroutine chain—where each coroutine takes the I/O object by reference and is awaited by a parent with the same affinity—synchronous code runs in the correct context because the caller is already there.

The problem arises at **task initiation boundaries**: when crossing from non-coroutine code (callbacks, event handlers, timers) into coroutine code via `spawn()` or `start()`.

```cpp
// Accept handler on the I/O thread—NOT on any strand
void on_accept(socket sock) {
    spawn(connection_handler(std::move(sock)));  // Task starts HERE
}

task connection_handler(socket sock) {
    setup_connection();       // Running in accept handler's context!
    validate_headers();       // Could race with I/O completions
    co_await sock.read(buf);  // Dispatcher kicks in HERE... too late
}
```

The `spawn` call starts the task in the accept handler's execution context. All synchronous code before the first `co_await` runs there—potentially racing with completions that arrive on the socket's strand. The `await_transform` mechanism only takes effect at suspension points; it cannot protect code that executes before any `co_await`.

**Implicit vs explicit strands.** Not every socket needs strand protection from the first instruction:

- **Implicit strand**: A freshly-created socket has singular ownership. No completions are pending; no other code has access. The "strand" is implicit in the fact that only one entity can touch the socket.

- **Explicit strand**: Once the socket is shared—stored in a container, passed to callbacks, accessible from completions—concurrent access becomes possible. A physical strand is needed from the first instruction.

The handoff to the coroutine is the boundary. Once `connection_handler` is spawned, both the coroutine body and I/O completions could execute concurrently.

**Resolution options.** Several approaches address this constraint:

- **Eager initial dispatch**: The task suspends at `initial_suspend` and resumes on the I/O object's dispatcher before any user code runs. This guarantees correct context but adds latency for tasks that don't need strand protection.

- **Caller responsibility**: Document that `spawn` must be called from the correct context. Simple but error-prone—the invariant is not enforced.

- **Lazy with immediate dispatch**: The task is lazy (returns without executing), but the first action upon `start()` is to dispatch to the correct context before resuming the coroutine body.

| Resource   | Must Be Available                  | Challenge                          |
|------------|------------------------------------|------------------------------------|
| Allocator  | At frame allocation (before body)  | Executor not yet injected          |
| Dispatcher | At first instruction               | Caller may be in wrong context     |

The allocator constraint is absolute—there is no later opportunity. The dispatcher constraint depends on whether the socket requires explicit strand protection and how the task is initiated.

---

## 6. The Executor

**Terminology note.** We use the term *executor* rather than *scheduler* intentionally. In `std::execution`, schedulers are designed for heterogeneous computing—selecting GPU vs CPU algorithms, managing completion domains, and dispatching to hardware accelerators. Networking has different needs: strand serialization, I/O completion contexts, and thread affinity. By using *executor*, we signal a distinct concept tailored to networking's requirements. This terminology also honors Chris Kohlhoff's executor model in Boost.Asio, which established the foundation for modern C++ asynchronous I/O.

C++20 coroutines provide type erasure *by construction*—but not through the handle type. `std::coroutine_handle<void>` and `std::coroutine_handle<promise_type>` are both just pointers with identical overhead. The erasure that matters is *structural*:

1. **The frame is opaque**: Callers see only a handle, not the promise's layout
2. **The return type is uniform**: All coroutines returning `task` have the same type, regardless of body
3. **Suspension points are hidden**: The caller doesn't know where the coroutine may suspend

This structural erasure is often lamented as overhead, but we recognize it as opportunity: *the allocation we cannot avoid can pay for the type erasure we need*.

A coroutine's promise can store execution context *by reference*, receiving it from the caller at suspension time rather than at construction time. This deferred binding enables a uniform `task` type that works with any executor, without encoding the executor type in its signature.

The executor is the minimal foundation for I/O workloads—a type that provides two operations: **dispatch** for continuations and **post** for independent work. Unlike `std::execution`'s scheduler, which is designed for algorithm selection across heterogeneous hardware, the executor is purpose-built for I/O: it manages completion contexts, serialization, and thread affinity.

```cpp
// Abbreviation for clarity of exposition
using coro = std::coroutine_handle<>;

struct executor_work_queue;

struct executor_work {
    virtual void operator()() = 0; // responsible for deleting this
    virtual void destroy() = 0;    // to destroy pending (uninvoked) work

protected:
    ~executor_work() = default;
    friend executor_work_queue;
    executor_work* next_;
};

// Abstract base class for executors
struct executor_base {
    virtual ~executor_base() = default;
    virtual coro dispatch( coro h ) const = 0;
    virtual void post( executor_work* w ) const = 0;

    // Frame allocation with default pass-through to global new/delete
    virtual void* allocate_frame( std::size_t n ) const { return ::operator new( n ); }
    virtual void deallocate_frame( void* p, std::size_t n) const { ::operator delete( p, n ); }

    // affine awaitable dispatcher
    coro operator()( coro h ) const { return dispatch( h ); }
};

// Executor concept
template< class T >
concept executor = std::derived_from< T, executor_base >;

// I/O object concept
template< class T >
concept io_object = requires( T const& t ) {
    { t.get_executor() } -> std::convertible_to< executor_base const& >;
};
```

### 6.1 Dispatch: Run This Continuation

`dispatch` schedules a coroutine handle for resumption. If the caller is already in the executor's context, the implementation may resume inline; otherwise, the handle is queued. We use `std::coroutine_handle<>` rather than a templated callable because the coroutine frame is already allocated and the handle already type-erased—both come for free.

When an I/O context thread dequeues a completion via `epoll_wait`, `GetQueuedCompletionStatus`, or `io_uring_wait_cqe`, it calls `dispatch` to resume the waiting coroutine. The return value enables symmetric transfer: rather than recursively calling `resume()`, the caller returns the handle to the coroutine machinery for a tail call, preventing stack overflow.

Some contexts prohibit inline execution. A strand currently executing work cannot dispatch inline without breaking serialization—`dispatch` then behaves like `post`, queuing unconditionally.

### 6.2 Post: Queue Independent Work

`post` queues work for later execution. Unlike `dispatch`, it never executes inline—the work item is always enqueued, and `post` returns immediately.

Use `post` for:
- **New work** that is not a continuation of the current operation
- **Breaking call chains** to bound stack depth
- **Safety under locks**—posting while holding a mutex avoids deadlock risk from inline execution

A coroutine-first design reveals a novel choice: the caller allocates work and provides type-erasure by deriving from `executor_work`. The model expects derived classes to delete themselves in `operator()`. This inverts the traditional model where the executor allocates and wraps a callable. The caller knows the work's lifetime and can embed it in existing storage—a coroutine frame, a connection object, a pre-allocated pool. The intrusive `next_` pointer in the base class enables lock-free queues without separate node allocation.

This ownership model enables an important optimization: a socket can preallocate its work object upon construction. Because the typical allocator, executor, and completion handler types are no longer stored in the operation state, the work object's size is fixed and known at compile time. The preallocated work simply updates its completion handler pointer before each submission. In this case, `operator()` invokes the handler but does not delete itself, `destroy()` does nothing, and the socket deletes the work in its destructor.

### 6.3 Allocation Policy

The executor owns allocation policy for coroutine frames. `allocate_frame` and `deallocate_frame` default to global `::operator new` and `::operator delete`. Concrete executors override these to provide:

- **Thread-local recycling pools** for zero steady-state allocations
- **Bounded pools** for real-time systems with predictable latency
- **Per-tenant budgets** to prevent resource exhaustion in multi-tenant servers
- **Allocation tracking** for debugging and profiling

I/O objects store an `executor_base*` and expose it via `get_executor()`. The promise's `operator new` detects `io_object` on arguments and calls `get_executor().allocate_frame(n)` for frame allocation.

### 6.4 Type Erasure and Composition

Executor types are fully preserved at call sites even though they're type-erased internally. This enables zero-overhead composition at the API boundary while maintaining uniform internal representation. The erasure boundary is pushed to where it matters least: internal propagation rather than the hot dispatch path.

**Composable executor wrappers work naturally:**

```cpp
template<class Ex>
struct strand {
    Ex inner_;
    
    coro dispatch(coro h) const {
        // Serialization logic, then delegate to inner
        return inner_.dispatch(h);
    }
    // ...
};

// Full type information at the call site
strand<pool_executor> s{pool.get_executor()};
async_run(s, my_task());  // strand<pool_executor> passed by value
```

When `async_run` stores the executor in the root task's frame, it preserves the complete type. The `strand` wrapper's serialization logic remains inlinable. Only when the executor propagates to child tasks—as `executor_base const&`—does type erasure occur.

**Executors encode policy, not just parallelism.** A single-threaded `io_context` still benefits from executor customization:

- **Serialization**: `strand` ensures operations on one connection don't interleave
- **Deferred execution**: `post` vs `dispatch` controls stack depth
- **Instrumentation**: Wrapper executors can measure latency, count operations, or propagate tracing context
- **Prioritization**: Critical control messages can preempt bulk data transfers
- **Testing**: Manual executors enable deterministic unit tests

**The coroutine advantage.** Traditional callback-based designs embed the executor in the I/O object's type (`basic_socket<Protocol, Executor>`), leaking concurrency policy into type identity. Coroutines invert this: the executor enters at `async_run`, propagates invisibly through `executor_base const*`, and reaches I/O operations via `await_transform`. The I/O object participates in executor selection without encoding it in its type:

```cpp
// Traditional: executor embedded in socket type
basic_socket<tcp, strand<pool_executor>> sock;

// Coroutine model: executor supplied at launch
socket sock;  // No executor type parameter
async_run(strand{pool.get_executor()}, use_socket(sock));
```

### 6.5 Executor Wrapping Trade-offs

For completeness, we acknowledge a case where callbacks are superior: executor wrappers that inject behavior around the posted work.

Callbacks can wrap work without allocation because the wrapper embeds the callable by value into existing operation state. Our model requires allocating a `work` wrapper to inject behavior—one allocation per wrapped post.

However, most practical executor wrappers—strands, thread pools, priority queues—do not need to wrap the work itself, only manage *when* and *where* it executes. For these, our model pays no penalty. The allocation cost appears only for exotic wrappers that must inject behavior around the work's execution.

---

## 7. Propagation Mechanisms

### 7.1 Allocator: Detection via Arguments

*TODO: io_object concept, resolution precedence: explicit → I/O object → global.*

### 7.2 Dispatcher: Forward Flow

*TODO: await_transform injection, resolution precedence: explicit → I/O object → inline.*

### 7.3 Why Not Thread-Local State

*TODO*

---

## 8. Context Propagation vs Backward Queries

### 8.1 The std::execution Query Model

*TODO*

### 8.2 Forward Flow Eliminates Queries

*TODO*

### 8.3 The Affine Awaitable Protocol

*TODO*

---

## 9. The std::execution Tax

If networking is required to integrate with `std::execution`, I/O libraries must pay a complexity tax regardless of whether they benefit from the framework's abstractions.

### 9.1 What I/O Libraries Must Implement

To participate in the sender/receiver ecosystem, networking code must implement:

- **Query protocol compliance**: `get_env`, `get_domain`, `get_completion_scheduler`—even if only to return defaults
- **Concept satisfaction**: Meet sender/receiver requirements designed for GPU algorithm dispatch
- **Transform machinery**: Domain transforms execute even when they select the only available implementation
- **API surface expansion**: Expose attributes and queries irrelevant to I/O operations

A socket returning `default_domain` still participates in the dispatch protocol. The P3826 machinery runs, finds no customization, and falls through to the default—overhead for nothing.

### 9.2 Type Leakage Through connect_result_t

The sender/receiver model solves a real problem: constructing a compile-time call graph for heterogeneous computation chains. When all types are visible at `connect()` time, the compiler can optimize across operation boundaries—inlining GPU kernel launches, eliminating intermediate buffers, and selecting optimal memory transfer strategies. For workloads where dispatch overhead is measured in nanoseconds and operations complete in microseconds, this visibility enables meaningful optimization.

Networking operates in a different regime. I/O latency is measured in tens of microseconds (NVMe storage) to hundreds of milliseconds (network round-trips). A 10-nanosecond dispatch optimization is irrelevant when the operation takes 100,000 nanoseconds. The compile-time call graph provides no benefit—there is no GPU kernel to inline, no heterogeneous dispatch to optimize.

Yet the sender/receiver model requires type visibility regardless:

```cpp
// std::execution pattern
execution::sender auto snd = socket.async_read(buf);
execution::receiver auto rcv = /* ... */;
auto state = execution::connect(snd, rcv);  // Type: connect_result_t<Sender, Receiver>
```

The `connect_result_t` type encodes the full operation state. Algorithms that compose senders must propagate these types:

```cpp
// From P2300: operation state types leak into composed operations
template<class S, class R>
struct _retry_op {
    using _child_op_t = stdexec::connect_result_t<S&, _retry_receiver<S, R>>;
    optional<_child_op_t> o_;  // Nested operation state, fully typed
};
```

For networking, this creates the template tax we sought to avoid (§2.1)—N×M instantiations, compile time growth, implementation details exposed through every API boundary—without the optimization payoff that justifies it for GPU workloads. Our design achieves zero type leakage; composed algorithms expose only `task` return types.

### 9.3 The Core Question

The question is not whether P2300/P3826 break networking code. They don't—defaults work. The question is whether networking should pay for abstractions it doesn't use.

| Abstraction | GPU/Parallel Need | Networking Need |
|-------------|-------------------|-----------------|
| Domain-based dispatch | Critical | None |
| Completion scheduler queries | Required | Unused |
| Sender transforms | Algorithm selection | Pass-through only |
| Typed operation state | Optimization opportunity | ABI liability |

Our analysis suggests the cost is not justified when a simpler, networking-native design achieves the same goals without the tax

---

## 10. Conclusion

We have presented an execution model designed from the ground up for coroutine-driven asynchronous I/O:

1. **Minimal executor abstraction**: Two operations—`dispatch` and `post`—suffice for I/O workloads. No domain queries, no algorithm customization, no completion scheduler inference.

2. **Clear responsibility model**: The executor decides allocation policy; the I/O object owns its executor. Both resources propagate forward through I/O object arguments.

3. **Complete type hiding**: Executor types do not leak into public interfaces. Platform I/O types remain hidden in translation units. Composed algorithms expose only `task` return types. This directly enables ABI stability.

4. **Forward context propagation**: Execution context flows with control flow, not against it. No backward queries. The affine awaitable protocol injects context through `await_transform`.

5. **Conscious tradeoff**: One pointer indirection per I/O operation (~1-2 nanoseconds) buys encapsulation, ABI stability, and fast compilation. For I/O-bound workloads where operations take 10,000+ nanoseconds, this cost is negligible.

The comparison with `std::execution` (§2.2) is instructive: that framework's complexity serves GPU workloads, not networking. P3826 adds machinery to fix problems networking doesn't have—domain-based algorithm dispatch, completion scheduler queries, sender transforms. Our design sidesteps these issues entirely because TCP servers don't run on GPUs.

This divergence suggests that **networking deserves first-class design consideration**, not adaptation to frameworks optimized for heterogeneous computing. The future of asynchronous C++ need not be a single universal abstraction—it may be purpose-built frameworks that excel at their primary use cases while remaining interoperable at the boundaries

---

## References

1. [N4242](https://wg21.link/n4242) — Executors and Asynchronous Operations, Revision 1 (2014)
2. [N4482](https://wg21.link/n4482) — Some notes on executors and the Networking Library Proposal (2015)
3. [P2300R10](https://wg21.link/p2300) — std::execution (Michał Dominiak, Georgy Evtushenko, Lewis Baker, Lucian Radu Teodorescu, Lee Howes, Kirk Shoop, Eric Niebler)
4. [P2762R2](https://wg21.link/p2762) — Sender/Receiver Interface for Networking (Dietmar Kühl)
5. [P3552R3](https://wg21.link/p3552) — Add a Coroutine Task Type (Dietmar Kühl, Maikel Nadolski)
6. [P3826R2](https://wg21.link/p3826) — Fix or Remove Sender Algorithm Customization (Lewis Baker, Eric Niebler)
7. [Boost.Asio](https://www.boost.org/doc/libs/release/doc/html/boost_asio.html) — Asynchronous I/O library (Chris Kohlhoff)

---

## Acknowledgements

This paper builds on the foundational work of many contributors to C++ asynchronous programming:

**Chris Kohlhoff** for Boost.Asio, which has served the C++ community for over two decades and established many of the patterns we build upon—and some we consciously depart from. The executor model in this paper honors his pioneering work.

**Lewis Baker** for his work on C++ coroutines, the Asymmetric Transfer blog series, and his contributions to P2300 and P3826. His explanations of symmetric transfer and coroutine optimization techniques directly informed our design.

**Dietmar Kühl** for P2762 and P3552, which explore sender/receiver networking and coroutine task types. His clear articulation of design tradeoffs—including the late-binding problem and cancellation overhead concerns—helped crystallize our understanding of where the sender model introduces friction for networking.

The analysis in this paper is not a critique of these authors' contributions, but rather an exploration of whether networking's specific requirements are best served by adapting to general-purpose abstractions or by purpose-built designs
