# DynamicBuffer Concept for Coroutine-First I/O

## Overview

This document proposes a modernized `DynamicBuffer` concept for Capy/Corosio that takes advantage of C++20 coroutine lifetime semantics. The design distinguishes between copyable buffer adapters (passed by value) and non-copyable buffers (passed by lvalue reference), enabling safe and ergonomic buffer usage in coroutine-based async code.

## Motivation

C++20 coroutines extend the lifetime of references bound to coroutine parameters until the coroutine completes. This creates an opportunity to design buffer APIs that are both safe and efficient:

- **Lvalue references** to buffers can be stored in the coroutine frame, with the caller responsible for ensuring the buffer outlives the coroutine
- **Copyable adapters** can be passed by value, with the adapter copied into the coroutine frame while the underlying storage remains in the caller's scope

The existing Asio `DynamicBuffer` concepts (v1 and v2) were designed for callback-based async programming and don't explicitly leverage these coroutine semantics.

## Tony Table: Concept Comparison

### Capy DynamicBuffer (Proposed)

**Concept Definition:**

```cpp
template<class T>
concept DynamicBuffer = requires(T& t, T const& ct, std::size_t n) {
    typename T::const_buffers_type;
    typename T::mutable_buffers_type;
    { ct.size() } -> std::convertible_to<std::size_t>;
    { ct.max_size() } -> std::convertible_to<std::size_t>;
    { ct.capacity() } -> std::convertible_to<std::size_t>;
    { ct.data() } -> std::same_as<typename T::const_buffers_type>;
    { t.prepare(n) } -> std::same_as<typename T::mutable_buffers_type>;
    t.commit(n);
    t.consume(n);
} &&
    ConstBufferSequence<typename T::const_buffers_type> &&
    MutableBufferSequence<typename T::mutable_buffers_type>;

template<class T>
concept DynamicBufferAdapter = DynamicBuffer<T> && std::copy_constructible<T>;
```

**Function Signature:**

```cpp
template<class B>
    requires DynamicBuffer<std::remove_cvref_t<B>> &&
             (std::is_lvalue_reference_v<B> || DynamicBufferAdapter<B>)
task<io_result<std::size_t>> read(io_stream& ios, B&& buffer);
```

**Usage:**

```cpp
std::string s;
co_await read(sock, dynamic_buffer(s));  // adapter by value

circular_buffer cb(buf);
co_await read(sock, cb);                  // lvalue ref
```

---

### Asio DynamicBuffer_v1

**Type Trait Definition:**

```cpp
template<typename T>
struct is_dynamic_buffer_v1 : /* SFINAE detection for:
    size(), max_size(), capacity(), data(), consume(),
    prepare(n), commit(n),
    const_buffers_type, mutable_buffers_type */ {};
```

**Function Signature:**

```cpp
template<typename DynamicBuffer_v1>
std::size_t read(SyncReadStream& s, DynamicBuffer_v1&& buffers,
    constraint_t<is_dynamic_buffer_v1<decay_t<DynamicBuffer_v1>>::value> = 0,
    constraint_t<!is_dynamic_buffer_v2<decay_t<DynamicBuffer_v1>>::value> = 0);
```

**Usage:**

```cpp
std::string s;
auto db = asio::dynamic_buffer(s);
read(sock, db);  // forwarding reference
```

---

### Asio DynamicBuffer_v2

**Type Trait Definition:**

```cpp
template<typename T>
struct is_dynamic_buffer_v2 : /* SFINAE detection for:
    size(), max_size(), capacity(), data(), consume(),
    grow(n), shrink(n),  // <-- Different from v1!
    const_buffers_type, mutable_buffers_type */ {};
```

**Function Signature:**

```cpp
template<typename DynamicBuffer_v2>
std::size_t read(SyncReadStream& s, DynamicBuffer_v2 buffers,  // by value!
    constraint_t<is_dynamic_buffer_v2<DynamicBuffer_v2>::value> = 0);
```

**Usage:**

```cpp
std::string s;
read(sock, asio::dynamic_buffer(s));  // passed by value
```

---

### Key Differences Summary

**Constraint Mechanism:**

- Capy: C++20 `concept` with clear, readable syntax
- Asio v1/v2: SFINAE `is_dynamic_buffer_vN<T>` traits (pre-C++20)

**Buffer Operations:**

- Capy/Asio v1: `prepare(n)` returns writable region, `commit(n)` finalizes
- Asio v2: `grow(n)` extends buffer, `shrink(n)` reduces (different model)

**Copyability:**

- Capy: Explicit `DynamicBufferAdapter` refinement for copy-safe types
- Asio v1/v2: No formal copyability distinction

**Parameter Passing:**

- Capy: Constrained universal ref (lvalue ref OR copyable rvalue)
- Asio v1: Forwarding reference `DynamicBuffer_v1&&`
- Asio v2: By value `DynamicBuffer_v2` (implicitly requires copyable)

**Coroutine Safety:**

- Capy: Designed for coroutine lifetime extension
- Asio v1/v2: Designed for callback-based async (no coroutine consideration)

## Proposed Concept Hierarchy

```
DynamicBuffer (base concept)
    │
    ├── DynamicBufferAdapter (refinement: + copy_constructible)
    │       └── dynamic_buffer(string&)
    │       └── dynamic_buffer(vector&)
    │       └── dynamic_buffer(circular_buffer&)
    │
    └── Non-copyable buffers (passed by lvalue reference only)
            └── string_buffer (move-only)
            └── custom user types
```

### Core Concept

The base `DynamicBuffer` concept requires the standard operations:

```cpp
template<class T>
concept DynamicBuffer = requires(T& t, T const& ct, std::size_t n) {
    typename T::const_buffers_type;
    typename T::mutable_buffers_type;
    { ct.size() } -> std::convertible_to<std::size_t>;
    { ct.max_size() } -> std::convertible_to<std::size_t>;
    { ct.capacity() } -> std::convertible_to<std::size_t>;
    { ct.data() } -> std::same_as<typename T::const_buffers_type>;
    { t.prepare(n) } -> std::same_as<typename T::mutable_buffers_type>;
    t.commit(n);
    t.consume(n);
} &&
    ConstBufferSequence<typename T::const_buffers_type> &&
    MutableBufferSequence<typename T::mutable_buffers_type>;
```

**Semantic Requirements:**

- `data()` and `prepare()` return buffer sequences valid until the next mutating operation
- Types may hold references to external storage (lifetime managed by user)

### DynamicBufferAdapter Refinement

```cpp
template<class T>
concept DynamicBufferAdapter = 
    DynamicBuffer<T> && 
    std::copy_constructible<T>;
```

This refinement identifies lightweight wrappers that can be safely copied into coroutine frames. The adapter itself is copied, while the underlying storage it references remains in the caller's scope.

### Factory Function

```cpp
// For growable containers (string, vector)
template<class Container>
auto dynamic_buffer(Container& c, std::size_t max_size = -1)
    -> dynamic_buffer_adapter<Container>;

// Overload for existing DynamicBuffer types
template<DynamicBuffer DB>
auto dynamic_buffer(DB& db) -> /* adapter wrapping pointer to db */;
```

### Function Signature Pattern

```cpp
template<class B>
    requires DynamicBuffer<std::remove_cvref_t<B>> &&
             (std::is_lvalue_reference_v<B> || DynamicBufferAdapter<B>)
task<io_result<std::size_t>> read(io_stream& ios, B&& buffer);
```

This accepts:

- Lvalue references to any `DynamicBuffer` (lifetime managed by caller)
- Rvalues of `DynamicBufferAdapter` (copied/moved into coroutine frame)

## Call-Site Examples

The following examples show what developers will actually write at call sites. These patterns are based on real-world usage in Beast and Asio.

### 1. Session Class with Member Buffer

The most common pattern: a session object owns the buffer as a member.

```cpp
class session : public std::enable_shared_from_this<session> {
    tcp_stream stream_;
    flat_buffer buffer_;  // persists across operations

public:
    task<void> run() {
        while (true) {
            // Buffer passed by lvalue reference - safe, lives in session
            auto [ec, n] = co_await read(stream_, buffer_);
            if (ec) 
                co_return;
            
            co_await write(stream_, buffer_.data());
            buffer_.consume(buffer_.size());
        }
    }
};
```

### 2. Stack-Local Buffer in Coroutine

Buffer declared locally in the coroutine function.

```cpp
task<void> handle_request(tcp_stream& stream) {
    flat_buffer buffer;  // local to coroutine
    
    // Passed by lvalue reference - coroutine lifetime extends buffer
    auto [ec, n] = co_await read(stream, buffer);
    if (ec) 
        co_return;
    
    process(buffer.data());
}
```

### 3. Reading into std::string with Adapter

Using `dynamic_buffer()` to wrap a growable string.

```cpp
task<std::string> read_all(tcp_stream& stream) {
    std::string result;
    
    // Adapter (copy) passed by value, string lives in coroutine frame
    auto [ec, n] = co_await read(stream, dynamic_buffer(result));
    
    co_return result;
}
```

### 4. Fixed-Size Stack Buffer with circular_buffer

Zero-allocation pattern using stack storage.

```cpp
task<void> echo_server(tcp_stream& stream) {
    char storage[8192];
    circular_buffer buffer(storage);
    
    while (true) {
        // Passed by lvalue reference
        auto [ec, n] = co_await read_some(stream, buffer.prepare(1024));
        if (ec) 
            co_return;
        buffer.commit(n);
        
        co_await write(stream, buffer.data());
        buffer.consume(buffer.size());
    }
}
```

### 5. WebSocket Message Loop

Pattern from Beast websocket examples.

```cpp
template<typename Stream>
task<void> websocket_session(Stream& stream, flat_buffer& buffer) {
    websocket_stream<Stream&> ws{stream};
    co_await ws.async_accept();
    
    while (true) {
        // Buffer from caller, passed by reference
        auto [ec, n] = co_await ws.async_read(buffer);
        if (ec == websocket::error::closed) 
            co_return;
        
        ws.text(ws.got_text());
        co_await ws.async_write(buffer.data());
        buffer.consume(buffer.size());
    }
}
```

### 6. HTTP Client Request

Pattern from Beast/Asio HTTP examples.

```cpp
task<http::response<http::string_body>> 
fetch(tcp_stream& stream, http::request<http::empty_body>& req) {
    // Send request
    co_await http::async_write(stream, req);
    
    // Receive response - buffer is local
    flat_buffer buffer;
    http::response<http::string_body> res;
    co_await http::async_read(stream, buffer, res);
    
    co_return res;
}
```

### 7. Read Until Delimiter

Line-reading pattern common in text protocols.

```cpp
task<std::string> read_line(tcp_stream& stream) {
    std::string line;
    
    // dynamic_buffer adapter wraps string
    auto [ec, n] = co_await read_until(
        stream, 
        dynamic_buffer(line), 
        '\n');
    
    co_return line;
}
```

### 8. What NOT to Write (Compile-Time Errors)

The concept constraint catches common mistakes at compile time.

```cpp
// WRONG: circular_buffer is not copyable, can't pass rvalue
task<void> bad_example(tcp_stream& stream) {
    char buf[512];
    co_await read(stream, circular_buffer(buf));  // compile error!
}
```

The constraint `(std::is_lvalue_reference_v<B> || DynamicBufferAdapter<B>)` rejects this because:
- `circular_buffer(buf)` is an rvalue (temporary)
- `circular_buffer` is not `copy_constructible`, so it doesn't satisfy `DynamicBufferAdapter`

The fix is to declare the buffer as a named variable:

```cpp
// CORRECT: pass by lvalue reference
task<void> good_example(tcp_stream& stream) {
    char buf[512];
    circular_buffer cb(buf);
    co_await read(stream, cb);  // OK - lvalue reference
}
```

### Lifetime Pitfall (Runtime Error - User Responsibility)

```cpp
// WRONG: buffer outlives its storage
task<void> lifetime_bug(tcp_stream& stream) {
    flat_buffer* buffer;
    {
        flat_buffer temp;
        buffer = &temp;
    }
    // temp is gone, buffer points to garbage
    co_await read(stream, *buffer);  // undefined behavior!
}
```

This is a standard C++ lifetime issue that cannot be caught at compile time. The concept doesn't prevent this, but the design makes the user's responsibility clear: if you pass by lvalue reference, you must ensure the buffer outlives the coroutine.

## Existing Buffer Types

The Capy library provides these `DynamicBuffer` implementations:

| Type | Copyable | Storage | Notes |
|------|----------|---------|-------|
| `flat_buffer` | Yes | External pointer | Single contiguous region |
| `circular_buffer` | Yes | External pointer | Ring buffer, 2-element sequences |
| `string_buffer` | No (move-only) | Wraps `std::string*` | Growable, resizes on prepare |

With the proposed design:
- `flat_buffer` and `circular_buffer` can be passed by lvalue reference directly
- `string_buffer` must be passed by lvalue reference (not copyable)
- `dynamic_buffer(string)` creates a copyable adapter that can be passed by value

## Discussion Points

### 1. Should `DynamicBufferAdapter` require `std::copyable` or just `std::copy_constructible`?

The current proposal uses `copy_constructible`. Using `copyable` (which adds `copy_assignable`) might be overly restrictive for adapters that are typically constructed once and passed.

### 2. Alternative: Two Overloads vs. Constrained Universal Reference

Instead of:
```cpp
template<class B>
    requires DynamicBuffer<std::remove_cvref_t<B>> &&
             (std::is_lvalue_reference_v<B> || DynamicBufferAdapter<B>)
task<...> read(io_stream& ios, B&& buffer);
```

We could use two overloads:
```cpp
template<DynamicBufferAdapter B>
task<...> read(io_stream& ios, B buffer);  // by value

template<DynamicBuffer B>
    requires (!DynamicBufferAdapter<B>)
task<...> read(io_stream& ios, B& buffer);  // by lvalue ref
```

Trade-offs:
- Single constrained template: simpler to maintain, one implementation
- Two overloads: clearer intent, but duplicated implementation

### 3. Should `dynamic_buffer()` return a reference wrapper for non-copyable types?

Currently, if `T` is a `DynamicBuffer` but not copyable, `dynamic_buffer(t)` could return a thin wrapper holding `T*`. This would allow:

```cpp
string_buffer sb(&str);
co_await read(sock, dynamic_buffer(sb));  // wrapper is copyable
```

This adds complexity but provides uniformity.

### 4. Compatibility with Asio

Should the concept be designed to also accept Asio's `dynamic_string_buffer` and similar types? This would ease migration but might constrain the design.

## Summary

The proposed `DynamicBuffer` concept hierarchy:

1. **Modernizes** the interface using C++20 concepts instead of SFINAE traits
2. **Explicitly distinguishes** copyable adapters from non-copyable buffers
3. **Leverages coroutine semantics** for safe and ergonomic buffer passing
4. **Provides compile-time safety** by rejecting dangerous rvalue usage of non-copyable buffers
5. **Maintains familiar patterns** from Beast and Asio for easy adoption

Feedback on this design is welcome before implementation proceeds.
