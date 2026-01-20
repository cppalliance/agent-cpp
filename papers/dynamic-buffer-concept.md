# DynamicBuffers Concept for Coroutine-First I/O

## Overview

This document proposes a modernized `DynamicBuffers` concept for Capy/Corosio using C++20 concepts, with a two-concept API that ensures coroutine-safe parameter passing.

## Naming Convention

This design uses **plural "buffers"** for dynamic buffer types to distinguish from singular "buffer" for buffer sequences:

- **Singular `buffer`** - byte spans and sequences (`const_buffer`, `mutable_buffer`, `ConstBufferSequence`)
- **Plural `buffers`** - dynamic buffer types (`flat_buffers`, `multi_buffers`, `DynamicBuffers`)

## Motivation

C++20 coroutines extend the lifetime of references bound to coroutine parameters until the coroutine completes. However, not all buffer types are safe to pass as rvalues:

- **Adapters** like `string_buffers` wrap external storage that retains state after the adapter is destroyed
- **Value types** like `flat_buffers` and `multi_buffers` hold bookkeeping that would be lost if the buffer is destroyed

The `DynamicBuffersParam` concept encodes this distinction, allowing:

- **Lvalues** of any `DynamicBuffers` type (caller manages lifetime)
- **Rvalues** only for types that opt-in as safe adapters

## Type Taxonomy

| Type | Description | Copyable | Safe as rvalue |
|------|-------------|----------|----------------|
| `string_buffers` | Adapts external `std::string` | yes | **yes** - string retains state via `.size()` |
| `flat_buffers` | Adapts external memory | yes | no - bookkeeping lives in wrapper |
| `multi_buffers` | Owns linked storage | yes | no - owns its data |
| `circular_buffers` | Adapts external `char[]` | yes | no - bookkeeping lives in wrapper |

**Key semantic property for rvalue safety**: After mutating operations (`commit`, `consume`), all relevant state is reflected in the external storage, not just in the wrapper.

For `string_buffers`, the underlying `std::string` itself tracks size and capacity - the wrapper is stateless. For `flat_buffers`, the wrapper holds offsets and sizes that the raw memory doesn't know about.

## Tony Table: Concept Comparison

### Capy DynamicBuffers (Proposed)

**Concept Definitions:**

```cpp
// Core concept - what buffer types implement
template<class T>
concept DynamicBuffers = requires(T& t, T const& ct, std::size_t n) {
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

// Parameter concept - what functions constrain on
template<class B>
concept DynamicBuffersParam = 
    DynamicBuffers<std::remove_cvref_t<B>> &&
    (std::is_lvalue_reference_v<B> || 
     requires { typename std::remove_cvref_t<B>::is_dynamic_buffers_adapter; });
```

**Function Signature:**

```cpp
template<DynamicBuffersParam B>
task<io_result<std::size_t>> read(io_stream& ios, B&& buffer);
```

**Usage:**

```cpp
std::string s;
co_await read(sock, string_buffers(s));  // rvalue adapter - OK

flat_buffers fb(storage);
co_await read(sock, fb);                  // lvalue - OK

co_await read(sock, flat_buffers(mem));   // rvalue non-adapter - compile error
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

- Capy: C++20 `concept` - clean, readable
- Asio v1/v2: SFINAE traits - verbose, pre-C++20

**Buffer Operations:**

- Capy/Asio v1: `prepare(n)` / `commit(n)` model
- Asio v2: `grow(n)` / `shrink(n)` model

**Parameter Passing:**

- Capy: Constrained forwarding reference (lvalue OR opted-in rvalue)
- Asio v1: Forwarding reference with SFINAE constraints
- Asio v2: By value (requires copyable)

**Coroutine Safety:**

- Capy: Explicit opt-in for rvalue-safe types via tag
- Asio v1/v2: No formal distinction

## The Concepts

### DynamicBuffers (Core Concept)

```cpp
template<class T>
concept DynamicBuffers = requires(T& t, T const& ct, std::size_t n) {
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

### DynamicBuffersParam (Parameter Concept)

```cpp
template<class B>
concept DynamicBuffersParam = 
    DynamicBuffers<std::remove_cvref_t<B>> &&
    (std::is_lvalue_reference_v<B> || 
     requires { typename std::remove_cvref_t<B>::is_dynamic_buffers_adapter; });
```

This concept encapsulates "is this a valid way to pass a DynamicBuffers to a coroutine?"

- If `B` is an lvalue reference type → always OK (caller manages lifetime)
- If `B` is not a reference type (rvalue) → must have the adapter tag

## Opt-in Mechanism

Types that are safe to pass as rvalues add a nested tag type:

```cpp
class string_buffers {
public:
    using is_dynamic_buffers_adapter = void;  // opt-in for rvalue safety
    
    // ... DynamicBuffers interface ...
};
```

This is self-contained (no namespace gymnastics for trait specialization) and self-documenting.

## Function Signature

```cpp
template<DynamicBuffersParam B>
task<io_result<std::size_t>> read(io_stream& ios, B&& buffer);
```

Clean, terse syntax. The concept handles all the constraint logic.

## Call-Site Behavior

| Call | `B` deduces to | Result |
|------|----------------|--------|
| `read(s, fb)` (lvalue) | `flat_buffers&` | OK - lvalue ref |
| `read(s, flat_buffers{...})` | `flat_buffers` | **compile error** - no adapter tag |
| `read(s, std::move(fb))` | `flat_buffers` | **compile error** - no adapter tag |
| `read(s, string_buffers(str))` | `string_buffers` | OK - has adapter tag |
| `read(s, dynamic_buffer(str))` | adapter type | OK - has adapter tag |

## Call-Site Examples

### 1. Session Class with Member Buffer

```cpp
class session : public std::enable_shared_from_this<session> {
    tcp_stream stream_;
    flat_buffers buffer_;

public:
    task<void> run() {
        while (true) {
            auto [ec, n] = co_await read(stream_, buffer_);
            if (ec) co_return;
            
            co_await write(stream_, buffer_.data());
            buffer_.consume(buffer_.size());
        }
    }
};
```

### 2. Stack-Local Buffer in Coroutine

```cpp
task<void> handle_request(tcp_stream& stream) {
    flat_buffers buffer;
    
    auto [ec, n] = co_await read(stream, buffer);
    if (ec) co_return;
    
    process(buffer.data());
}
```

### 3. Reading into std::string with Adapter

```cpp
task<std::string> read_all(tcp_stream& stream) {
    std::string result;
    
    // string_buffers has adapter tag - safe as rvalue
    auto [ec, n] = co_await read(stream, string_buffers(result));
    
    co_return result;
}
```

### 4. Fixed-Size Stack Buffer

```cpp
task<void> echo_server(tcp_stream& stream) {
    char storage[8192];
    circular_buffers buffer(storage);
    
    while (true) {
        auto [ec, n] = co_await read_some(stream, buffer.prepare(1024));
        if (ec) co_return;
        buffer.commit(n);
        
        co_await write(stream, buffer.data());
        buffer.consume(buffer.size());
    }
}
```

### 5. WebSocket Message Loop

```cpp
template<typename Stream>
task<void> websocket_session(Stream& stream, flat_buffers& buffer) {
    websocket_stream<Stream&> ws{stream};
    co_await ws.async_accept();
    
    while (true) {
        auto [ec, n] = co_await ws.async_read(buffer);
        if (ec == websocket::error::closed) co_return;
        
        ws.text(ws.got_text());
        co_await ws.async_write(buffer.data());
        buffer.consume(buffer.size());
    }
}
```

### 6. HTTP Client Request

```cpp
task<http::response<http::string_body>> 
fetch(tcp_stream& stream, http::request<http::empty_body>& req) {
    co_await http::async_write(stream, req);
    
    flat_buffers buffer;
    http::response<http::string_body> res;
    co_await http::async_read(stream, buffer, res);
    
    co_return res;
}
```

### 7. Read Until Delimiter

```cpp
task<std::string> read_line(tcp_stream& stream) {
    std::string line;
    
    auto [ec, n] = co_await read_until(stream, string_buffers(line), '\n');
    
    co_return line;
}
```

### 8. What NOT to Write

```cpp
// WRONG: flat_buffers passed as rvalue - compile error
task<void> bad_example(tcp_stream& stream) {
    char buf[512];
    co_await read(stream, flat_buffers(buf));  // ERROR: no adapter tag
}

// WRONG: buffer outlives its storage
task<void> another_bad_example(tcp_stream& stream) {
    flat_buffers* buffer;
    {
        flat_buffers temp;
        buffer = &temp;
    }
    // temp is gone, buffer points to garbage
    co_await read(stream, *buffer);  // undefined behavior!
}
```

## Existing Buffer Types

| Type | Copyable | Safe as rvalue | Storage | Notes |
|------|----------|----------------|---------|-------|
| `string_buffers` | Yes | **Yes** | Wraps `std::string&` | String retains state |
| `flat_buffers` | Yes | No | External pointer | Wrapper holds offsets |
| `circular_buffers` | Yes | No | External pointer | Ring buffer |
| `multi_buffers` | Yes | No | Owns linked list | Value type |

## Discussion Points

### 1. Factory Function

Should `dynamic_buffer()` be provided for wrapping containers like `std::string` and `std::vector`?

```cpp
template<class Container>
auto dynamic_buffer(Container& c, std::size_t max_size = -1);
```

This would return an adapter type with the `is_dynamic_buffers_adapter` tag.

### 2. Compatibility with Asio

Should the concept accept Asio's `dynamic_string_buffer` and similar types? They would satisfy `DynamicBuffers` but not have the adapter tag, so they'd only work as lvalues.

## Summary

The proposed design:

1. Uses C++20 `concept` for clean constraints
2. Distinguishes buffer sequences (singular) from dynamic buffers (plural) in naming
3. Two public concepts: `DynamicBuffers` (what types implement) and `DynamicBuffersParam` (what functions use)
4. Tag-based opt-in for rvalue-safe adapter types
5. Compile-time enforcement of safe parameter passing patterns
6. Maintains familiar patterns from Beast and Asio

Feedback welcome.
