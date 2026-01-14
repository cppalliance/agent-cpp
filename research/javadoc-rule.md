# C++ Library Documentation Principles

Write documentation comments following these conventions.

## 1. Document Structure

### 1.1 Required Sections

Every library should provide:

- **Introduction** — Purpose, scope, design philosophy
- **Quick Start** — Minimal working example
- **Tutorial** — Task-oriented guidance for common use cases  
- **Reference** — Complete API specification
- **Rationale** — Design decisions and trade-offs
- **Examples** — Compilable, self-contained programs
- **Changelog** — Version history with migration notes

### 1.2 Reference Page Context

Each reference page must be self-contained for search engine discovery:

- Fully qualified identifiers (`namespace::class::member`)
- Required headers for each entity
- Links to related tutorial content
- Links to relevant examples
- Navigation to library root


## 2. Function Documentation Elements

Document functions using the C++ Standard Library element ordering. Include only applicable elements:

| Element | Purpose |
|---------|---------|
| **Synopsis** | Declaration with template parameters |
| **Constraints** | SFINAE / `requires` clause conditions |
| **Mandates** | Compile-time requirements (ill-formed if violated) |
| **Preconditions** | Runtime requirements on arguments/state (UB if violated) |
| **Effects** | What the function does (excluding postconditions, returns, throws) |
| **Synchronization** | Thread-safety and ordering guarantees |
| **Postconditions** | Predicates true after successful return |
| **Returns** | Value returned (not type—that's in the synopsis) |
| **Throws** | Exception type and condition that triggers it |
| **Exception Safety** | Guarantee level: nothrow, strong, or basic |
| **Complexity** | Time/space bounds |
| **Remarks** | Additional normative requirements |
| **Notes** | Informative, non-normative guidance |
| **Example** | Brief usage demonstration |
| **See Also** | Related functions/classes |

### 2.1 Element Guidelines

**Preconditions**: Express as predicates. Do not repeat type requirements already enforced by the language.

```
Preconditions: `first != last`.
```

**Effects**: Describe only actions not covered by other elements. If the entire effect is captured by Returns/Throws/Postconditions, omit Effects.

**Postconditions**: Express as C++ predicates when possible.

```
Postconditions: `size() == old_size + 1`.
```

**Throws**: Specify both the exception type and triggering condition.

```
Throws: `std::length_error` if `n > max_size()`.
```

**Returns**: State the value, not the type. Briefs for return-value functions should begin with "Return".

```cpp
/** Return the number of elements in the container. */
size_type size() const noexcept;
```


## 3. Exception Safety Documentation

Every function must document its exception safety guarantee:

| Guarantee | Meaning |
|-----------|---------|
| **Nothrow** | Never throws. Required for destructors, `swap`, move operations used by strong-guarantee code. Mark `noexcept`. |
| **Strong** | If an exception is thrown, program state is unchanged (commit-or-rollback). |
| **Basic** | If an exception is thrown, invariants are preserved and no resources leak. Objects remain in a valid but unspecified state. |

Document the guarantee explicitly:

```
Exception Safety: Strong guarantee.
```

For conditional guarantees:

```
Exception Safety: Strong guarantee if `T` is nothrow-move-constructible; 
otherwise basic guarantee.
```


## 4. Template Parameter Documentation

### 4.1 Concept Requirements

Specify requirements using named concepts or requirement tables:

```cpp
/** Serialize a range to the output stream.
 *
 *  @tparam Range     Type satisfying `std::ranges::input_range`
 *  @tparam CharT     Character type of the output stream
 */
template<std::ranges::input_range Range, typename CharT>
void serialize(std::basic_ostream<CharT>& os, Range const& r);
```

### 4.2 Requirement Tables

For custom concepts, provide a requirements table:

| Expression | Return Type | Semantics |
|------------|-------------|-----------|
| `*a` | `T&` | Dereferences the iterator |
| `++a` | `Iterator&` | Advances to next element |

### 4.3 Variadic Templates

Do not use `@tparam` for variadic parameter packs (`Args...`). Document them in prose or through `@param` for the corresponding function parameters.


## 5. Class Documentation

### 5.1 Class Invariants

Document what must always be true for valid objects of the class:

```
Class Invariants:
  - `data_ != nullptr || size_ == 0`
  - `size_ <= capacity_`
```

### 5.2 Thread Safety

Document thread safety at the class level:

```
Thread Safety:
  Distinct objects: Safe.
  Shared objects: Unsafe. External synchronization required for concurrent access.
```

Or for specific guarantees:

```
Thread Safety:
  All const member functions are safe for concurrent access.
  Non-const member functions require exclusive access.
```


## 6. Asynchronous Operation Documentation

For completion-handler-based async operations (Asio style):

### 6.1 Required Elements

```cpp
/** Asynchronously read data from the stream.
 *
 *  @param buffer     The buffer to read into. Ownership is not transferred;
 *                    the caller must ensure the buffer remains valid until
 *                    the completion handler is invoked.
 *
 *  @param handler    The completion handler. The signature must be:
 *                    @code
 *                    void handler(
 *                        error_code ec,           // Result of operation
 *                        std::size_t bytes_transferred  // Bytes read
 *                    );
 *                    @endcode
 *
 *  @par Completion Signature
 *  @code void(error_code, std::size_t) @endcode
 *
 *  @par Per-Operation Cancellation
 *  Supports `cancellation_type::terminal`, `cancellation_type::partial`,
 *  and `cancellation_type::total`.
 */
template<typename MutableBufferSequence, typename ReadHandler>
auto async_read_some(MutableBufferSequence const& buffer, ReadHandler&& handler);
```

### 6.2 Lifetime Requirements

Always document object lifetime requirements for async operations:

```
The caller must ensure that the stream, buffer, and any referenced objects 
remain valid until the completion handler is invoked or the operation is cancelled.
```


## 7. Code Examples

### 7.1 Requirements

- **Compilable**: Every example must compile with documented compiler flags
- **Self-contained**: Include all necessary headers
- **Minimal**: Demonstrate one concept per example
- **Correct**: Examples are tested as part of CI

### 7.2 Synopsis Blocks

Provide a synopsis showing the class/function interface:

```cpp
namespace lib {

template<typename T, typename Allocator = std::allocator<T>>
class dynamic_buffer
{
public:
    // Types
    using value_type      = T;
    using allocator_type  = Allocator;
    using size_type       = std::size_t;
    
    // Constructors
    dynamic_buffer();
    explicit dynamic_buffer(allocator_type const& alloc);
    
    // Capacity
    size_type size() const noexcept;
    size_type capacity() const noexcept;
    
    // Modifiers
    void push_back(T const& value);
    void push_back(T&& value);
};

} // namespace lib
```


## 8. Javadoc/Doxygen Style Guide

### 8.1 Brief Descriptions

- Functions returning values: Begin with "Return"
- Predicates: Begin with "Check" or state what is tested
- Mutators: Begin with verb describing action
- Constructors: "Construct" or "Create"

```cpp
/** Return the current size of the buffer. */
size_type size() const noexcept;

/** Check if the container is empty. */
bool empty() const noexcept;

/** Append an element to the end of the container. */
void push_back(T const& value);

/** Construct an empty container. */
container();
```

### 8.2 Tag Usage

```cpp
/**
 *  @brief      Brief one-line description.
 *
 *  @details    Extended description spanning multiple paragraphs
 *              if necessary.
 *
 *  @tparam T   Element type. Must satisfy `std::copyable`.
 *
 *  @param x    Description of parameter x.
 *  @param y    Description of parameter y.
 *
 *  @return     Description of return value.
 *
 *  @throws std::bad_alloc  If memory allocation fails.
 *
 *  @pre        `x != nullptr`
 *  @post       `size() > 0`
 *
 *  @note       Additional non-normative information.
 *
 *  @warning    Important caveat or danger.
 *
 *  @see        related_function()
 *  @see        RelatedClass
 *
 *  @since      1.2.0
 */
```

### 8.3 Formatting

- Use `@` prefix (not `\`) for Doxygen commands
- Use backticks for inline code in descriptions
- Use `@code`/`@endcode` for multi-line code blocks
- Keep lines under 80 characters in doc comments

### 8.4 Full Comment Structure Example

```cpp
/** Brief one-sentence description ending with a period.

    Extended description providing more detail. Can span multiple
    paragraphs. Use backticks for `code references` inline.

    @par Preconditions
    @li Condition that must be true before calling.

    @par Effects
    Description of what the function does (the observable behavior).

    @par Postconditions
    @li Condition guaranteed true after successful return.

    @par Exception Safety
    Strong guarantee / Basic guarantee / No-throw guarantee.

    @par Thread Safety
    Thread-safe / Not thread-safe / Const operations are thread-safe.

    @par Complexity
    O(n) where n is the size of the container.

    @throws std::invalid_argument If condition.

    @note Important implementation detail or caveat.

    @par Example
    @code
    auto result = my_function(arg1, arg2);
    @endcode

    @tparam T Non-deduced template parameter description.

    @param name Single-line parameter description.
    @param other Another parameter.

    @return Description of return value.

    @see related_function
*/
```

Constructors and destructors don't have Thread Safety sections.

### 8.5 Section Order

1. Brief + extended description
2. `@par` sections (Preconditions, Effects, Postconditions, Exception Safety, Thread Safety, Complexity)
3. `@throws`
4. `@note`
5. `@par Example`
6. `@tparam` (non-deduced only)
7. `@param`
8. `@return`
9. `@see` (last)

### 8.6 Style Rules

- **Brief description**: First sentence, concise, ends with period
- **Extended description**: After blank line, can be multiple paragraphs
- **@tparam**: Only for non-deduced template parameters (omit if deduced from arguments)
- **@param entries**: Single line each
- **@par sections**: Only include if relevant (omit empty sections)
- **Code references**: Use backticks for types, functions, parameters
- **Wrap lines**: Around 80 characters, indent continuation by 4 spaces

### 8.7 Section Guidelines

| Section | Include When |
|---------|--------------|
| Preconditions | Function has requirements on inputs or state |
| Effects | Behavior isn't obvious from name/signature |
| Postconditions | Guarantees beyond "returns X" |
| Exception Safety | Function can throw or has specific guarantees |
| Thread Safety | Class/function used in concurrent contexts |
| Complexity | Non-trivial algorithmic complexity |


## 9. Version and Compatibility

### 9.1 API Versioning

Document minimum language standard:

```
Requires: C++17 or later.
```

### 9.2 Deprecation

```cpp
/** @deprecated Use new_function() instead. Will be removed in version 3.0. */
[[deprecated("use new_function() instead")]]
void old_function();
```

### 9.3 Feature Macros

Document feature-test macros and configuration options:

```
Defined if compiled with `MYLIB_ENABLE_FEATURE_X`.
```


## 10. Cross-References

### 10.1 Internal Links

Link to related components within the library:

```
@see buffer_sequence
@see basic_stream
```

### 10.2 External References

Reference external standards and specifications:

```
@par Specification
  RFC 7230, Section 3.2.6
```


## 11. Common Patterns

### 11.1 Customization Points

Document ADL-based customization points:

```cpp
/** Swap two objects.
 *
 *  @par Customization
 *  To customize for user-defined type `T`, provide a function
 *  `swap(T&, T&)` discoverable via ADL in the same namespace as `T`.
 */
```

### 11.2 Type Traits

```cpp
/** Determine if a type satisfies the Buffer concept.
 *
 *  @tparam T   Type to check.
 *
 *  Provides member `value` equal to `true` if `T` satisfies Buffer,
 *  otherwise `false`.
 *
 *  @par Member Types
 *  | Name         | Definition                |
 *  |--------------|---------------------------|
 *  | `value_type` | `bool`                    |
 *  | `type`       | `std::integral_constant`  |
 */
template<typename T>
struct is_buffer;
```


## 12. Writing Principles

### 12.1 Precision Over Brevity

Terse descriptions that sacrifice correctness are worse than longer precise ones.

### 12.2 Normative vs. Informative

- Normative text (Requirements, Effects, etc.) defines behavior
- Informative text (Notes, Examples) aids understanding but does not define behavior
- Mark informative sections explicitly

### 12.3 Testable Statements

Requirements and postconditions should be expressible as testable predicates.

### 12.4 No Implementation Details

Document the interface contract, not the implementation. Exceptions:
- Performance characteristics users rely upon
- Platform-specific behavior
- Constraints imposed by implementation choices


## Appendix A: Exception Safety Checklist

For each function, verify:

- [ ] Exception safety level documented
- [ ] All throwing operations identified  
- [ ] Resource cleanup guaranteed (RAII)
- [ ] Strong guarantee: state unchanged on throw
- [ ] Nothrow operations marked `noexcept`
- [ ] Destructor is nothrow


## Appendix B: Async Operation Checklist

For each async operation, verify:

- [ ] Completion handler signature documented
- [ ] Lifetime requirements for all parameters
- [ ] Cancellation support documented
- [ ] Error conditions enumerated
- [ ] Intermediate handler requirements (if any)
- [ ] Associated executor/allocator behavior


## Appendix C: Quick Reference

**Brief style by function category:**

| Category | Brief starts with |
|----------|------------------|
| Accessor returning value | "Return" |
| Predicate | "Check" / "Test" |
| Mutator | Action verb |
| Constructor | "Construct" |
| Destructor | "Destroy" / "Destruct" |
| Conversion | "Convert" |
| Factory | "Create" / "Make" |
| Async initiator | "Asynchronously" + verb |
