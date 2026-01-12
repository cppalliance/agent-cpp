# Boost.CallableTraits Library Documentation

Boost.CallableTraits provides a comprehensive set of metafunctions for inspecting, synthesizing, and decomposing callable types in C++11 and later.

> Code snippets throughout the documentation are written as if the following declarations are in effect:
> ```cpp
> #include <boost/callable_traits.hpp>
> namespace ct = boost::callable_traits;
> ```

## Introduction

Boost.CallableTraits is a header-only library that provides type traits for callable types—functions, function pointers, function references, member function pointers, member data pointers, and function objects. The library eliminates the need for tedious template specializations when working with function signatures.

### What This Library Does

- Extracts return types, parameter lists, and class types from callable types
- Adds or removes member qualifiers (`const`, `volatile`, `&`, `&&`)
- Adds or removes `noexcept` specifier (C++17)
- Adds or removes C-style varargs (`...`)
- Converts between function types, pointers, and member pointers
- Provides `is_invocable` as a standalone C++11 implementation
- Supports `transaction_safe` (Transactional Memory TS)

### What This Library Does Not Do

- Does not provide runtime callable wrappers (use `std::function` for that)
- Does not deduce types from generic lambdas or overloaded function objects
- Does not handle platform-specific calling conventions (e.g., `__stdcall`)

### Design Philosophy

Working with function types in C++ requires handling numerous signature variations: `const`, `volatile`, lvalue/rvalue reference qualifiers, `noexcept`, varargs, and `transaction_safe`. A complete set of template specializations for all combinations requires 48+ specializations in C++17—and that's just for plain function types. CallableTraits eliminates this complexity by providing a unified interface that works across all callable types.

### Requirements

- **C++ Standard:** C++11 or later
- **Build Type:** Header-only (no compilation required)
- **Dependencies:** None (does not require Boost)
- **Compiler Support:** GCC 4.7+, Clang 3.5+, Visual Studio 2015+

## Quick Start

The following example demonstrates basic usage with function objects and member function pointers:

```cpp
#include <boost/callable_traits.hpp>
#include <type_traits>
#include <tuple>

namespace ct = boost::callable_traits;

struct foo {
    void operator()(int, char, float) const {}
};

int main()
{
    // Extract parameter list as std::tuple
    static_assert(std::is_same<
        ct::args_t<foo>,
        std::tuple<int, char, float>
    >::value, "");

    // Check for void return type
    static_assert(ct::has_void_return<foo>::value, "");

    // Check for C-style varargs
    static_assert(!ct::has_varargs<foo>::value, "");

    // Work with member function pointers
    using pmf = decltype(&foo::operator());

    // Remove const qualifier
    static_assert(std::is_same<
        ct::remove_member_const_t<pmf>,
        void (foo::*)(int, char, float)
    >::value, "");

    // Add const qualifier
    static_assert(std::is_same<
        ct::add_member_const_t<void (foo::*)(int, char, float)>,
        pmf
    >::value, "");

    // Check for const member
    static_assert(ct::is_const_member<pmf>::value, "");
}
```

## Supported Types

CallableTraits works with the following callable types:

| Type Category | Example |
|---------------|---------|
| Function | `void(int, char)` |
| Function pointer | `void(*)(int, char)` |
| Function reference | `void(&)(int, char)` |
| Member function pointer | `void(foo::*)(int)` |
| Member data pointer | `int foo::*` |
| Function object | `struct { void operator()(); }` |
| Lambda (non-generic) | `[](int x){ return x; }` |

**Constraints:** Generic lambdas and overloaded `operator()` are not supported because the parameter types cannot be deduced without explicit instantiation.

## Inspection Traits

### return_type

Extracts the return type from a callable:

```cpp
using F = int(float, char);
static_assert(std::is_same<ct::return_type_t<F>, int>::value, "");

using PMF = int(foo::*)(float) const;
static_assert(std::is_same<ct::return_type_t<PMF>, int>::value, "");
```

| Input | `return_type_t<T>` |
|-------|---------------------|
| `void()` | `void` |
| `float(*)()` | `float` |
| `const char*(&)()` | `const char*` |
| `int(foo::*)() const` | `int` |
| `int` | (substitution failure) |

### args

Extracts parameter types into a container (default: `std::tuple`):

```cpp
using F = void(int, char, float);
static_assert(std::is_same<
    ct::args_t<F>,
    std::tuple<int, char, float>
>::value, "");

// Use a custom container
template<class...> struct my_list {};
static_assert(std::is_same<
    ct::args_t<F, my_list>,
    my_list<int, char, float>
>::value, "");
```

For member function pointers, the first argument is a reference to the class, qualified appropriately:

| Input | `args_t<T>` |
|-------|-------------|
| `void(float, char, int)` | `std::tuple<float, char, int>` |
| `void(foo::*)(float, char, int)` | `std::tuple<foo&, float, char, int>` |
| `int(foo::*)(int) const` | `std::tuple<const foo&, int>` |
| `void(foo::*)() volatile &&` | `std::tuple<volatile foo&&>` |
| `int foo::*` | `std::tuple<const foo&>` |

### function_type

Extracts the function type, normalizing member pointers to free functions:

```cpp
using PMF = int(foo::*)(float) const;
static_assert(std::is_same<
    ct::function_type_t<PMF>,
    int(const foo&, float)
>::value, "");
```

| Input | `function_type_t<T>` |
|-------|----------------------|
| `void(int)` | `void(int)` |
| `void(int) const` | `void(int)` |
| `void(*const&)(int)` | `void(int)` |
| `int(foo::*)(int)` | `int(foo&, int)` |
| `int(foo::*)(int) const` | `int(const foo&, int)` |
| `void(foo::*)() volatile &&` | `void(volatile foo&&)` |
| `int foo::*` | `int(const foo&)` |

### class_of

Extracts the class type from a member pointer:

```cpp
using PMF = int(foo::*)(float) const;
static_assert(std::is_same<ct::class_of_t<PMF>, foo>::value, "");

using PMD = int foo::*;
static_assert(std::is_same<ct::class_of_t<PMD>, foo>::value, "");
```

### qualified_class_of

Extracts the class type with member qualifiers as a reference type:

```cpp
using PMF = int(foo::*)(float) const;
static_assert(std::is_same<ct::qualified_class_of_t<PMF>, const foo&>::value, "");

using PMF2 = int(foo::*)() volatile &&;
static_assert(std::is_same<ct::qualified_class_of_t<PMF2>, volatile foo&&>::value, "");
```

## Boolean Traits

### has_void_return

Checks if the return type is `void`:

```cpp
static_assert(ct::has_void_return<void()>::value, "");
static_assert(!ct::has_void_return<int()>::value, "");
```

### has_varargs

Checks for C-style variadic arguments (`...`):

```cpp
static_assert(ct::has_varargs<int(char, ...)>::value, "");
static_assert(!ct::has_varargs<int(char)>::value, "");
```

### has_member_qualifiers

Checks for any member qualifier (`const`, `volatile`, `&`, `&&`):

```cpp
static_assert(ct::has_member_qualifiers<int() const>::value, "");
static_assert(ct::has_member_qualifiers<int() &>::value, "");
static_assert(!ct::has_member_qualifiers<int()>::value, "");
```

### is_const_member / is_volatile_member / is_cv_member

Checks for specific cv-qualifiers:

```cpp
static_assert(ct::is_const_member<int() const>::value, "");
static_assert(ct::is_volatile_member<int() volatile>::value, "");
static_assert(ct::is_cv_member<int() const volatile>::value, "");
```

### is_lvalue_reference_member / is_rvalue_reference_member / is_reference_member

Checks for reference qualifiers:

```cpp
static_assert(ct::is_lvalue_reference_member<int() &>::value, "");
static_assert(ct::is_rvalue_reference_member<int() &&>::value, "");
static_assert(ct::is_reference_member<int() &>::value, "");
static_assert(ct::is_reference_member<int() &&>::value, "");
```

### is_noexcept (C++17)

Checks for the `noexcept` specifier:

```cpp
static_assert(ct::is_noexcept<int() noexcept>::value, "");
static_assert(!ct::is_noexcept<int()>::value, "");
```

**Note:** Returns `false` on compilers without C++17 `noexcept` support.

### is_transaction_safe

Checks for the `transaction_safe` specifier (Transactional Memory TS):

```cpp
// Only available with compiler TM support (e.g., GCC with -fgnu-tm)
static_assert(ct::is_transaction_safe<int() transaction_safe>::value, "");
```

### is_invocable / is_invocable_r

Standalone C++11 implementation of C++17 `std::is_invocable`:

```cpp
struct foo {
    void operator()(int) {}
};

static_assert(ct::is_invocable<foo, int>::value, "");
static_assert(!ct::is_invocable<foo, int, int>::value, "");

// Check return type convertibility
static_assert(ct::is_invocable_r<void, foo, int>::value, "");
```

## Type Transformation Traits

### Adding Member Qualifiers

Add qualifiers to function types or member function pointers:

| Trait | Description |
|-------|-------------|
| `add_member_const_t<T>` | Add `const` member qualifier |
| `add_member_volatile_t<T>` | Add `volatile` member qualifier |
| `add_member_cv_t<T>` | Add `const volatile` member qualifier |
| `add_member_lvalue_reference_t<T>` | Add `&` member qualifier |
| `add_member_rvalue_reference_t<T>` | Add `&&` member qualifier |

```cpp
using F = int(foo::*)();
static_assert(std::is_same<ct::add_member_const_t<F>, int(foo::*)() const>::value, "");
static_assert(std::is_same<ct::add_member_volatile_t<F>, int(foo::*)() volatile>::value, "");
static_assert(std::is_same<ct::add_member_lvalue_reference_t<F>, int(foo::*)() &>::value, "");
```

**Reference Collapsing:** Adding `&` to `&&` yields `&`, following standard reference collapsing rules:

| Input | `add_member_lvalue_reference_t<T>` |
|-------|-----------------------------------|
| `int()` | `int() &` |
| `int() const` | `int() const &` |
| `int() &&` | `int() &` |

### Removing Member Qualifiers

Remove qualifiers from function types or member function pointers:

| Trait | Description |
|-------|-------------|
| `remove_member_const_t<T>` | Remove `const` member qualifier |
| `remove_member_volatile_t<T>` | Remove `volatile` member qualifier |
| `remove_member_cv_t<T>` | Remove `const volatile` member qualifiers |
| `remove_member_reference_t<T>` | Remove `&` or `&&` member qualifier |

```cpp
using F = int(foo::*)() const volatile &&;
static_assert(std::is_same<ct::remove_member_const_t<F>, int(foo::*)() volatile &&>::value, "");
static_assert(std::is_same<ct::remove_member_reference_t<F>, int(foo::*)() const volatile>::value, "");
static_assert(std::is_same<ct::remove_member_cv_t<F>, int(foo::*)() &&>::value, "");
```

### add_noexcept / remove_noexcept (C++17)

Add or remove the `noexcept` specifier:

```cpp
using F = int();
static_assert(std::is_same<ct::add_noexcept_t<F>, int() noexcept>::value, "");

using G = int() noexcept;
static_assert(std::is_same<ct::remove_noexcept_t<G>, int()>::value, "");
```

**Note:** On compilers without C++17 `noexcept` support, `add_noexcept_t` causes substitution failure, and `remove_noexcept_t` has no effect.

### add_varargs / remove_varargs

Add or remove C-style variadic arguments:

```cpp
using F = int(char);
static_assert(std::is_same<ct::add_varargs_t<F>, int(char, ...)>::value, "");

using G = int(char, ...);
static_assert(std::is_same<ct::remove_varargs_t<G>, int(char)>::value, "");
```

### apply_member_pointer

Convert a type to a member pointer of a specified class:

```cpp
using F = int(float);
static_assert(std::is_same<ct::apply_member_pointer_t<F, foo>, int(foo::*)(float)>::value, "");

using P = int(*)(float);
static_assert(std::is_same<ct::apply_member_pointer_t<P, foo>, int(foo::*)(float)>::value, "");

// Non-function types become member data pointers
static_assert(std::is_same<ct::apply_member_pointer_t<int, foo>, int foo::*>::value, "");
```

| Input | `apply_member_pointer_t<T, foo>` |
|-------|----------------------------------|
| `int()` | `int(foo::*)()` |
| `int(&)()` | `int(foo::*)()` |
| `int(*)()` | `int(foo::*)()` |
| `int(bar::*)()` | `int(foo::*)()` |
| `int(bar::*)() const` | `int(foo::*)() const` |
| `int bar::*` | `int foo::*` |
| `int` | `int foo::*` |

### apply_return

Change the return type of a callable:

```cpp
using F = int(float, char);
static_assert(std::is_same<ct::apply_return_t<F, void>, void(float, char)>::value, "");

using PMF = int(foo::*)(float) const;
static_assert(std::is_same<ct::apply_return_t<PMF, char>, char(foo::*)(float) const>::value, "");
```

Special case with `std::tuple`:

```cpp
using Args = std::tuple<int, float>;
static_assert(std::is_same<ct::apply_return_t<Args, void>, void(int, float)>::value, "");
```

## Variable Templates

When supported by the compiler, boolean traits have `_v` suffixes:

```cpp
static_assert(ct::has_void_return_v<void()>, "");
static_assert(ct::is_const_member_v<int() const>, "");
static_assert(ct::is_noexcept_v<int() noexcept>, "");
static_assert(ct::is_invocable_v<foo, int>, "");
```

## SFINAE Compatibility

All traits cause substitution failure when constraints are violated, enabling use in SFINAE contexts:

```cpp
template<typename F>
auto invoke_if_void_return(F&& f) -> std::enable_if_t<ct::has_void_return_v<F>>
{
    f();
}

template<typename F>
auto invoke_if_void_return(F&& f) -> std::enable_if_t<!ct::has_void_return_v<F>, ct::return_type_t<F>>
{
    return f();
}
```

## Abominable Function Types

"Abominable" function types are function types with cv/ref qualifiers that cannot be the type of an actual function:

```cpp
using Abom = int() const &&;  // Valid type, but no function can have this type
```

These types are primarily useful for member function pointer manipulation. Some older compilers (GCC 4.7-4.8) do not support abominable function types. Define `BOOST_CLBL_TRTS_DISABLE_ABOMINABLE_FUNCTIONS` to avoid compilation errors on such compilers.

## Comparison with Boost.FunctionTypes

CallableTraits offers several advantages over the older Boost.FunctionTypes library:

| Feature | CallableTraits | FunctionTypes |
|---------|----------------|---------------|
| Dependencies | None | Boost.MPL |
| Function objects | First-class support | Not supported |
| Ref qualifiers (`&`, `&&`) | Supported | Not supported |
| `noexcept` | Supported (C++17) | Not supported |
| `transaction_safe` | Supported (with TM) | Not supported |
| API style | `<type_traits>` style | Tag-based |

### Example: Removing const

**FunctionTypes:**
```cpp
#include <boost/function_types/components.hpp>
#include <boost/function_types/member_function_pointer.hpp>

using const_removed = typename boost::function_types::member_function_pointer<
    typename boost::function_types::components<decltype(&foo::bar)>::types,
    boost::function_types::non_const>::type;
```

**CallableTraits:**
```cpp
#include <boost/callable_traits.hpp>

using const_removed = ct::remove_member_const_t<decltype(&foo::bar)>;
```

## API Reference Summary

### Type Extraction

| Trait | Description |
|-------|-------------|
| `return_type_t<T>` | Return type of callable |
| `args_t<T, Container>` | Parameter types in Container (default: `std::tuple`) |
| `function_type_t<T>` | Normalized function type |
| `class_of_t<T>` | Class type from member pointer |
| `qualified_class_of_t<T>` | Qualified class reference type |

### Boolean Queries

| Trait | Description |
|-------|-------------|
| `has_void_return<T>` | Return type is `void` |
| `has_varargs<T>` | Has C-style varargs |
| `has_member_qualifiers<T>` | Has any member qualifier |
| `is_const_member<T>` | Has `const` qualifier |
| `is_volatile_member<T>` | Has `volatile` qualifier |
| `is_cv_member<T>` | Has `const volatile` qualifiers |
| `is_lvalue_reference_member<T>` | Has `&` qualifier |
| `is_rvalue_reference_member<T>` | Has `&&` qualifier |
| `is_reference_member<T>` | Has `&` or `&&` qualifier |
| `is_noexcept<T>` | Has `noexcept` specifier |
| `is_transaction_safe<T>` | Has `transaction_safe` specifier |
| `is_invocable<T, Args...>` | Can invoke T with Args |
| `is_invocable_r<R, T, Args...>` | Can invoke T with Args returning R |

### Type Transformation

| Trait | Description |
|-------|-------------|
| `add_member_const_t<T>` | Add `const` qualifier |
| `add_member_volatile_t<T>` | Add `volatile` qualifier |
| `add_member_cv_t<T>` | Add `const volatile` qualifiers |
| `add_member_lvalue_reference_t<T>` | Add `&` qualifier |
| `add_member_rvalue_reference_t<T>` | Add `&&` qualifier |
| `remove_member_const_t<T>` | Remove `const` qualifier |
| `remove_member_volatile_t<T>` | Remove `volatile` qualifier |
| `remove_member_cv_t<T>` | Remove `const volatile` qualifiers |
| `remove_member_reference_t<T>` | Remove `&` or `&&` qualifier |
| `add_noexcept_t<T>` | Add `noexcept` specifier |
| `remove_noexcept_t<T>` | Remove `noexcept` specifier |
| `add_varargs_t<T>` | Add C-style varargs |
| `remove_varargs_t<T>` | Remove C-style varargs |
| `add_transaction_safe_t<T>` | Add `transaction_safe` specifier |
| `remove_transaction_safe_t<T>` | Remove `transaction_safe` specifier |
| `apply_member_pointer_t<T, C>` | Convert T to member pointer of C |
| `apply_return_t<T, R>` | Change return type to R |

### Struct vs Alias Templates

Each trait is available in two forms:

```cpp
// Alias template (preferred)
using Result = ct::return_type_t<F>;

// Struct form (for dependent contexts)
using Result = typename ct::return_type<F>::type;
```

The struct form is useful when the type argument is dependent and you need to defer evaluation.
