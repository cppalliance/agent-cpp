---
description: Create or update Javadoc documentation for a C++ header file and its corresponding source file
---

# Javadoc Documentation Command

Create or update documentation for the current file. For header files, also find and document the corresponding source file.

## Scope

**Public API (headers)** - Javadoc style (`/**` or `///`):

- Classes and structs
- Enums and enum classes
- Type aliases (`using`, `typedef`)
- Functions (free functions, member functions)
- Constants and variables
- Concepts (C++20)

**Implementation (headers and source files)** - Light `//` comments:

- One or two lines explaining the "why" of non-obvious or surprising code
- Helps LLMs and humans quickly understand motivation

**Automatic pairing**: When run on a header file (`.hpp`, `.h`), also find and document its corresponding source file (`.cpp`, `.cc`) in the same directory or `src/` directory.

## Brief Style

Start the brief based on function category:

| Category | Brief starts with |
|----------|-------------------|
| Accessor returning value | "Return" |
| Predicate | "Check" / "Test" |
| Mutator | Action verb |
| Constructor | "Construct" |
| Destructor | "Destroy" |
| Conversion | "Convert" |
| Factory | "Create" / "Make" |
| Async initiator | "Asynchronously" + verb |

## Section Order

For full Javadocs, use this order (include only relevant sections):

1. Brief (first line after `/**`, ends with period)
2. Extended description - explain what problem this solves
3. `@par` sections: Preconditions, Effects, Postconditions, Exception Safety, Thread Safety, Complexity
4. `@throws`
5. `@note`
6. `@par Example`
7. `@tparam` (non-deduced template parameters only)
8. `@param`
9. `@return`
10. `@see`

## Content Guidelines

- **Explain the problem first** - After the brief, explain what problem the class or function solves
- **Avoid stating the obvious** - Don't document inheritance or nested types unless critical
- **Classes need good examples** - Include practical usage examples
- **Use `//` comments inside Javadocs** - C-style `/* */` don't nest; prefer `//` in `@code` blocks
- **Boilerplate gets brief-only** - Use `///` for trivial functions (iterators, `empty()`, `size()`, operators)
- **Consult implementation, never expose it** - Look at code to understand behavior, but document the interface contract, not how it works. Documentation must remain correct if the implementation changes

## Tags

Use `@` prefix (not `\`): `@param`, `@return`, `@pre`, `@post`, `@throws`, `@note`, `@see`, `@code`/`@endcode`, `@tparam`

**@tparam**: Only for template parameters NOT deduced from function arguments. Skip for variadic packs (`Args...`) or types evident from `@param`.

## Examples

### Brief-only (trivial functions)

```cpp
/// Return true if the container is empty.
bool empty() const noexcept;

/// Return the number of elements.
size_type size() const noexcept;

/// Advance to the next element.
iterator& operator++();
```

### Full Javadoc (non-trivial functions)

```cpp
/** Insert an element at the specified position.

    Elements after the insertion point are shifted. Invalidates
    iterators at or after the insertion point.

    @par Exception Safety
    Strong guarantee.

    @par Complexity
    Linear in the number of elements after `pos`.

    @param pos Position before which the element will be inserted.
    @param value The value to insert.

    @return Iterator to the inserted element.

    @throws std::bad_alloc If memory allocation fails.
*/
iterator insert(const_iterator pos, T const& value);
```

### Class documentation

```cpp
/** A thread pool for executing work concurrently.

    Use this when you need to distribute CPU-bound tasks across
    multiple threads without the overhead of creating and destroying
    threads for each task.

    @par Thread Safety
    Distinct objects: Safe.
    Shared objects: Safe.

    @par Example
    @code
    thread_pool pool(4);  // 4 worker threads
    pool.post([] {
        // work runs on pool thread
    });
    pool.join();  // wait for all work
    @endcode
*/
class thread_pool;
```

### Implementation comments

```cpp
void thread_pool::worker_thread()
{
    // Pin to CPU to reduce cache thrashing on NUMA systems
    set_thread_affinity(thread_id_);

    for (;;)
    {
        // Check stop flag before blocking to avoid missed wakeups
        if (stop_.load(std::memory_order_acquire))
            break;

        auto task = queue_.pop();
        if (task)
            task();
    }
}
```

## Formatting

- Use backticks for inline code: `size()`, `nullptr`, `std::vector`
- Wrap lines around 80 characters
- Indent continuation by 4 spaces
