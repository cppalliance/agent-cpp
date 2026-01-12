# Boost.LEAF Library Documentation

Boost.LEAF (Lightweight Error Augmentation Framework) provides a portable, efficient error handling library for C++11.

> Code snippets throughout the documentation are written as if the following declarations are in effect:
> ```cpp
> #include <boost/leaf.hpp>
> namespace leaf = boost::leaf;
> ```

## Introduction

Boost.LEAF provides a decomposed approach to error handling where `result<T>` objects carry only success/failure discriminants and optional values, while error objects are communicated directly to error handling scopes via thread-local storage. This design enables constant-time error propagation independent of call stack depth.

### What This Library Does

- Provides `result<T>` for lightweight success/failure indication
- Communicates error objects directly to handlers without storing them in results
- Supports both exception-based and exception-free error handling
- Enables selective error handling with pattern matching via predicates
- Offers deferred error loading via `on_error` for RAII-style error augmentation
- Provides comprehensive diagnostics for debugging
- Works in embedded environments with configurable features

### What This Library Does Not Do

- Does not store error objects inside `result<T>` (by design)
- Does not require dynamic memory allocation for error handling
- Does not impose inheritance hierarchies on error types
- Does not require error types to have any particular interface

### Design Philosophy

Traditional result types like `result<T, E>` store both the value and error together. LEAF observes that callers typically only need to know *if* an error occurred, not *what* the error is, until reaching an error handling scope. By separating the discriminant from error objects, LEAF achieves:

1. Extremely lightweight `result<T>` with no coupling to error types
2. Constant-time error communication regardless of call stack depth
3. Efficient handling of large error objects without heap allocation

### Requirements

- **C++ Standard:** C++11 or later
- **Build Type:** Header-only (single-header available)
- **Dependencies:** None (does not require Boost)
- **Compiler Support:** GCC 4.8+, Clang 3.9+, Visual Studio 2015+

## Quick Start

The following program demonstrates basic error reporting, propagation, and handling:

```cpp
#include <boost/leaf.hpp>
#include <iostream>
#include <string>

namespace leaf = boost::leaf;

enum class ConversionError
{
    EmptyString,
    IllegalChar,
    TooLong
};

leaf::result<int> convert(std::string const& str)
{
    if (str.empty())
        return leaf::new_error(ConversionError::EmptyString);

    if (str.length() > 9)
        return leaf::new_error(ConversionError::TooLong);

    for (char c : str)
        if (!std::isdigit(c))
            return leaf::new_error(ConversionError::IllegalChar);

    return std::atoi(str.c_str());
}

int main()
{
    return leaf::try_handle_all(
        []() -> leaf::result<int>
        {
            BOOST_LEAF_AUTO(value, convert("42"));
            std::cout << "Result: " << value << std::endl;
            return 0;
        },
        [](leaf::match<ConversionError, ConversionError::EmptyString>)
        {
            std::cerr << "Empty string!" << std::endl;
            return 1;
        },
        [](leaf::match<ConversionError, ConversionError::IllegalChar>)
        {
            std::cerr << "Illegal character!" << std::endl;
            return 2;
        },
        [](ConversionError e)
        {
            std::cerr << "Other conversion error" << std::endl;
            return 3;
        },
        [](leaf::error_info const& unmatched)
        {
            std::cerr << "Unknown error: " << unmatched << std::endl;
            return 4;
        });
}
```

The program creates errors with `new_error`, propagates them through `result<T>`, and handles them with type-matched handlers in `try_handle_all`.

## Reporting Errors

### Creating Errors

Use `new_error` to create an error and return it from a function:

```cpp
leaf::result<int> parse_number(std::string const& s)
{
    if (s.empty())
        return leaf::new_error(e_empty_string{});

    // Parse and return value
    return std::stoi(s);
}
```

Pass any number of error objects to `new_error`:

```cpp
return leaf::new_error(
    e_parse_error{},
    e_file_name{filename},
    e_at_line{line_number}
);
```

### Error Objects

Error objects can be any type. LEAF provides several common ones:

| Type | Description |
|------|-------------|
| `e_errno` | Captures `errno` value |
| `e_file_name` | Stores filename as `std::string` |
| `e_api_function` | Stores API function name |
| `e_type_info_name` | Stores type name string |
| `e_at_line` | Stores line number |
| `e_source_location` | Stores file, line, and function name |
| `windows::e_LastError` | Captures Windows `GetLastError()` |

Custom error types should be simple structs with a `value` member or printable via `operator<<`:

```cpp
struct e_connection_id
{
    int value;
};

struct e_error_message
{
    std::string value;

    friend std::ostream& operator<<(std::ostream& os, e_error_message const& e)
    {
        return os << e.value;
    }
};
```

### Source Location

Use `BOOST_LEAF_NEW_ERROR` to automatically include source location:

```cpp
leaf::result<void> operation()
{
    if (failed)
        return BOOST_LEAF_NEW_ERROR(e_operation_failed{});
    return {};
}
```

The `e_source_location` type captures file, line, and function name.

## The Result Type

### Basic Usage

`leaf::result<T>` holds either a value of type `T` or an error discriminant:

```cpp
leaf::result<int> r = 42;       // Success with value
leaf::result<int> r2 = leaf::new_error(e_failed{}); // Failure
```

Check for success and access the value:

```cpp
if (r)
{
    int value = r.value();      // Safe access
    int value2 = *r;            // Operator* (asserts on error)
}

if (r.has_error())
{
    // Handle error case
    return r.error();           // Propagate error
}
```

### Void Results

`leaf::result<void>` indicates success or failure with no value:

```cpp
leaf::result<void> initialize()
{
    if (failed)
        return leaf::new_error(e_init_failed{});
    return {};  // Success
}
```

### Loading Additional Error Objects

Add error objects to an existing error using `load`:

```cpp
leaf::result<int> outer()
{
    auto r = inner();
    if (!r)
        return r.error().load(e_context{"in outer function"});
    return r;
}
```

### Converting Between Result Types

Error results convert automatically between different `result<T>` instantiations:

```cpp
leaf::result<std::string> process()
{
    leaf::result<int> r = get_value();
    if (!r)
        return r.error();  // Converts to result<std::string>
    return std::to_string(*r);
}
```

## Error Propagation

### BOOST_LEAF_AUTO

Extracts the value on success or returns the error:

```cpp
leaf::result<int> compute()
{
    BOOST_LEAF_AUTO(a, get_first());   // Returns error if get_first() fails
    BOOST_LEAF_AUTO(b, get_second());  // Returns error if get_second() fails
    return a + b;
}
```

Equivalent to:

```cpp
leaf::result<int> compute()
{
    auto r1 = get_first();
    if (!r1)
        return r1.error();
    auto a = r1.value();
    
    auto r2 = get_second();
    if (!r2)
        return r2.error();
    auto b = r2.value();
    
    return a + b;
}
```

### BOOST_LEAF_CHECK

For `result<void>` or when the value is not needed:

```cpp
leaf::result<void> initialize_all()
{
    BOOST_LEAF_CHECK(init_subsystem_a());
    BOOST_LEAF_CHECK(init_subsystem_b());
    return {};
}
```

On GCC/Clang, `BOOST_LEAF_CHECK` can also be used in expressions:

```cpp
leaf::result<float> compute()
{
    return process(BOOST_LEAF_CHECK(get_input()));
}
```

### BOOST_LEAF_ASSIGN

Assigns to an existing variable:

```cpp
int value;
BOOST_LEAF_ASSIGN(value, get_value());  // Assigns or returns error
```

## Error Handling

### try_handle_all

Handles all possible errors; the last handler must match any error:

```cpp
int result = leaf::try_handle_all(
    []() -> leaf::result<int>
    {
        BOOST_LEAF_AUTO(x, operation());
        return x;
    },
    [](e_specific_error const& e)
    {
        // Handle specific error
        return -1;
    },
    [](leaf::error_info const&)
    {
        // Catch-all handler (required)
        return -2;
    });
```

The try-block must return `result<T>`. The function returns `T` (not `result<T>`).

### try_handle_some

Handles some errors; unhandled errors propagate:

```cpp
leaf::result<int> result = leaf::try_handle_some(
    []() -> leaf::result<int>
    {
        return operation();
    },
    [](e_recoverable const& e) -> leaf::result<int>
    {
        // Handle recoverable error
        return recover();
    });
    // Unhandled errors remain in result
```

Handlers can return `result<T>` to indicate they couldn't handle the error.

### try_catch

For exception-based error handling:

```cpp
int result = leaf::try_catch(
    []
    {
        might_throw();
        return 0;
    },
    [](std::runtime_error const& e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    },
    [](leaf::error_info const&)
    {
        return 2;
    });
```

### Handler Arguments

Handlers receive error objects by declaring them as parameters:

| Parameter Type | Behavior |
|----------------|----------|
| `E` | Handler called if error object of type `E` is available |
| `E const&` | Same as `E`, passed by const reference |
| `E*` | Receives pointer to `E` or `nullptr` if unavailable |
| `match<E, V...>` | Handler called if `E` is available and matches values |
| `catch_<Ex...>` | Handler called if exception of type `Ex` was caught |
| `error_info const&` | Always available; contains error ID and exception info |
| `diagnostic_info const&` | Always available; includes diagnostic output |

### Handler Selection

LEAF selects the first handler whose arguments are all available:

```cpp
leaf::try_handle_all(
    try_block,
    [](e_error_a, e_error_b)  // Called only if BOTH are available
    {
        // Handle when both errors present
    },
    [](e_error_a a)  // Called if e_error_a available (even without e_error_b)
    {
        // Handle e_error_a only
    },
    []()  // Catch-all
    {
    });
```

## Predicates

### match

Tests if an error object equals specific values:

```cpp
[](leaf::match<Error, Error::NotFound, Error::Timeout> m)
{
    // Called if Error is NotFound or Timeout
    Error e = m.matched;  // Access the matched error
}
```

### match_value

Tests the `value` member of an error object:

```cpp
struct e_errno { int value; };

[](leaf::match_value<e_errno, ENOENT, EACCES>)
{
    // Called if e_errno.value is ENOENT or EACCES
}
```

### match_member (C++17)

Tests a specific member of an error object:

```cpp
struct e_error { int code; std::string msg; };

[](leaf::match_member<&e_error::code, 404, 500>)
{
    // Called if e_error.code is 404 or 500
}
```

### condition

Matches `std::error_code` against error conditions:

```cpp
[](leaf::match<leaf::condition<std::errc>, std::errc::no_such_file_or_directory>)
{
    // Called if error_code matches this condition
}
```

### if_not

Negates a predicate:

```cpp
[](leaf::if_not<leaf::match<Error, Error::Success>>)
{
    // Called if Error is present but NOT Success
}
```

### catch_

Matches exception types:

```cpp
[](leaf::catch_<std::runtime_error> e)
{
    std::cerr << e.matched.what() << std::endl;
}
```

## Deferred Error Loading

### on_error

Use `on_error` to load error objects only when an error occurs:

```cpp
leaf::result<void> process_file(std::string const& filename)
{
    auto load = leaf::on_error(e_file_name{filename});
    
    BOOST_LEAF_CHECK(open_file());   // If this fails, e_file_name is loaded
    BOOST_LEAF_CHECK(parse_file());  // If this fails, e_file_name is loaded
    return {};
}
```

The error object is loaded only if an error is detected when `load` goes out of scope.

### Deferred Computation

Pass a lambda to compute the error object lazily:

```cpp
auto load = leaf::on_error(
    []{ return e_timestamp{std::time(nullptr)}; }
);
```

### Accumulating Errors

Pass a lambda taking a reference to accumulate into an error object:

```cpp
auto load = leaf::on_error(
    [](e_error_trace& trace)
    {
        trace.value.push_back(__FILE__);
    }
);
```

### Error Logging Example

```cpp
#define ERROR_LOG \
    auto _log = leaf::on_error( \
        [](e_error_log& log) { \
            log << e_error_log::rec{__FILE__, __LINE__}; \
        })

leaf::result<void> f1()
{
    ERROR_LOG;
    if (failure)
        return leaf::new_error();
    return {};
}
```

## Exception Integration

### Throwing LEAF Exceptions

Use `throw_exception` or `BOOST_LEAF_THROW_EXCEPTION`:

```cpp
// With automatic source location
BOOST_LEAF_THROW_EXCEPTION(std::runtime_error("failed"), e_context{ctx});

// Without source location
leaf::throw_exception(std::runtime_error("failed"), e_context{ctx});
```

### exception_to_result

Converts exceptions to results:

```cpp
leaf::result<int> safe_parse(std::string const& s)
{
    return leaf::exception_to_result<std::invalid_argument>(
        [&]{ return std::stoi(s); });
}
```

The specified exception types are captured as error objects.

### try_capture_all

Captures all errors (including exceptions) into a result for later handling:

```cpp
leaf::result<int> captured = leaf::try_capture_all(
    []() -> leaf::result<int>
    {
        return operation_that_might_throw();
    });

// Handle captured errors later, possibly on another thread
```

## Diagnostics

### error_info

Always available, provides basic error information:

```cpp
[](leaf::error_info const& info)
{
    std::cerr << "Error ID: " << info.error() << std::endl;
    if (auto* ex = info.exception())
        std::cerr << "Exception: " << ex->what() << std::endl;
}
```

### diagnostic_info

Includes information about available error objects:

```cpp
[](leaf::diagnostic_info const& info)
{
    std::cerr << info << std::endl;
    // Prints error ID, exception info, and available error objects
}
```

### diagnostic_details

Most verbose output, includes captured error objects:

```cpp
[](leaf::diagnostic_details const& details)
{
    std::cerr << details << std::endl;
    // Prints everything including dynamically captured errors
}
```

## Configuration

### Compile-Time Options

| Macro | Default | Description |
|-------|---------|-------------|
| `BOOST_LEAF_CFG_DIAGNOSTICS` | 1 | Enable diagnostic output |
| `BOOST_LEAF_CFG_STD_SYSTEM_ERROR` | 1 | Enable `std::error_code` support |
| `BOOST_LEAF_CFG_STD_STRING` | 1 | Enable `std::string` support |
| `BOOST_LEAF_CFG_CAPTURE` | 1 | Enable error capture across threads |
| `BOOST_LEAF_CFG_WIN32` | 0 | Enable Windows-specific features |
| `BOOST_LEAF_NO_EXCEPTIONS` | (auto) | Disable exception support |
| `BOOST_LEAF_EMBEDDED` | - | Minimal configuration for embedded |

### Embedded Configuration

Define `BOOST_LEAF_EMBEDDED` to disable features for resource-constrained environments:

```cpp
#define BOOST_LEAF_EMBEDDED
#include <boost/leaf.hpp>
```

This sets diagnostics, system_error, string, and capture to 0.

### Thread-Local Storage

LEAF uses thread-local storage by default. Alternative TLS implementations:

| Header | Use Case |
|--------|----------|
| `tls_cpp11.hpp` | C++11 thread_local (default) |
| `tls_array.hpp` | Custom array-based TLS |
| `tls_freertos.hpp` | FreeRTOS task-local storage |
| `tls_globals.hpp` | Single-threaded (no TLS) |

## Caveats and Limitations

### Thread Safety

Error objects are stored in thread-local storage. Each thread has independent error state. Use `try_capture_all` to transfer errors between threads.

### No Copy Semantics

`result<T>` is move-only. Error state cannot be copied, only moved.

### Handler Ordering

Handlers are considered in order. More specific handlers should come before general ones:

```cpp
leaf::try_handle_all(
    try_block,
    [](leaf::match<Error, Error::Specific>){},  // Specific first
    [](Error){},                                  // General second
    [](){}                                        // Catch-all last
);
```

### Error Objects Must Be Available

Handler arguments must have corresponding error objects loaded at the error site. LEAF cannot retrieve error objects that were never loaded.

### Last Handler Requirement

`try_handle_all` requires the last handler to accept any error (via `error_info const&` or no arguments).

## API Reference Summary

### Error Creation

| Function | Description |
|----------|-------------|
| `new_error(E...)` | Create new error with error objects |
| `current_error()` | Get current error ID |

### Result Operations

| Member | Description |
|--------|-------------|
| `has_value()` / `operator bool()` | Check for success |
| `has_error()` | Check for failure |
| `value()` | Access stored value (throws on error) |
| `operator*()` | Access value (asserts on error) |
| `operator->()` | Pointer access to value |
| `error()` | Get error for propagation |
| `load(E...)` | Add error objects to error |

### Error Handling Functions

| Function | Description |
|----------|-------------|
| `try_handle_all(try_block, handlers...)` | Handle all errors, return value |
| `try_handle_some(try_block, handlers...)` | Handle some errors, return result |
| `try_catch(try_block, handlers...)` | Handle exceptions |
| `try_capture_all(try_block)` | Capture errors for later handling |
| `exception_to_result<Ex...>(f)` | Convert exceptions to result |

### Macros

| Macro | Description |
|-------|-------------|
| `BOOST_LEAF_AUTO(var, expr)` | Extract value or return error |
| `BOOST_LEAF_CHECK(expr)` | Check result, return on error |
| `BOOST_LEAF_ASSIGN(var, expr)` | Assign value or return error |
| `BOOST_LEAF_NEW_ERROR(E...)` | Create error with source location |
| `BOOST_LEAF_THROW_EXCEPTION(E...)` | Throw with source location |

### Predicate Types

| Type | Description |
|------|-------------|
| `match<E, V...>` | Match error type with specific values |
| `match_value<E, V...>` | Match error's value member |
| `match_member<P, V...>` | Match specific member (C++17) |
| `condition<E, Enum>` | Match error_code condition |
| `if_not<P>` | Negate predicate |
| `catch_<Ex...>` | Match exception types |

### Diagnostic Types

| Type | Description |
|------|-------------|
| `error_info` | Basic error information |
| `diagnostic_info` | Error info with type details |
| `diagnostic_details` | Full diagnostic output |
