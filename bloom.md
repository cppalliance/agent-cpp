# Boost.Bloom Library Documentation

Boost.Bloom provides a high-performance, configurable Bloom filter implementation for C++.

> Code snippets throughout the documentation are written as if the following declarations are in effect:
> ```cpp
> #include <boost/bloom.hpp>
> using namespace boost::bloom;
> ```

## Introduction

Boost.Bloom provides the class template `filter` that implements classical Bloom filters and variations discussed in the literature. A Bloom filter is a probabilistic data structure where inserted elements can be looked up with 100% accuracy, but looking up a non-inserted element may incorrectly return true. The probability of these false positives is called the false positive rate (FPR).

### What This Library Does

- Implements classical Bloom filters with configurable parameters
- Provides block filters and multiblock filters for improved performance
- Supports SIMD acceleration via AVX2, SSE2, and Neon instruction sets
- Offers compile-time configuration of filter behavior through template parameters
- Enables filter combination through bitwise OR and AND operations
- Provides FPR estimation and capacity calculation utilities

### What This Library Does Not Do

- Does not support element deletion (Bloom filters are insert-only by design)
- Does not track the number of inserted elements
- Does not provide thread-safe operations (users must provide external synchronization)
- Does not include counting Bloom filter variants

### Design Philosophy

Bloom filters occupy much less space than traditional containers. Typical configurations use 8-20 bits per element while maintaining acceptably low false positive rates. The library prioritizes performance through SIMD optimizations while offering flexible configuration options. Users specify filter parameters at compile time, enabling aggressive compiler optimizations.

### Requirements

- **C++ Standard:** C++11 or later
- **Build Type:** Header-only (no compilation required)
- **Dependencies:** Boost.ContainerHash, Boost.Config, Boost.Core
- **Compiler Support:** GCC 4.8+, Clang 3.9+, Visual Studio 2015+

## Quick Start

The following program creates a Bloom filter, inserts elements, and performs lookups:

```cpp
#include <boost/bloom.hpp>
#include <cassert>
#include <iostream>
#include <string>

int main()
{
    // Bloom filter of strings with 5 bits set per insertion
    using filter = boost::bloom::filter<std::string, 5>;

    // Create filter with a capacity of 1,000,000 bits
    filter f(1000000);

    // Insert elements into the filter
    f.insert("hello");
    f.insert("Boost");

    // Elements inserted are always correctly identified
    assert(f.may_contain("hello") == true);

    // Elements not inserted may yield false positives
    if (f.may_contain("bye")) {
        std::cout << "false positive\n";
    }
    else {
        std::cout << "element not found\n";
    }
}
```

The filter stores no actual data. Insertion sets bits in an internal array based on hash values. Lookup checks whether those bits are set. False positives occur when unrelated insertions happen to set the same bits.

## Filter Configuration

The `filter` class template accepts six parameters:

```cpp
template<
    typename T,
    std::size_t K,
    typename Subfilter = block<unsigned char, 1>,
    std::size_t Stride = 0,
    typename Hash = boost::hash<T>,
    typename Allocator = std::allocator<unsigned char>
>
class filter;
```

### Template Parameters

| Parameter | Description |
|-----------|-------------|
| `T` | Type of elements inserted into the filter |
| `K` | Number of subarrays marked per insertion |
| `Subfilter` | Strategy for setting bits within each subarray |
| `Stride` | Distance in bytes between consecutive subarrays (0 = automatic) |
| `Hash` | Hash function for elements of type `T` |
| `Allocator` | Allocator for the internal bit array |

The total number of bits set per insertion equals `K * K'`, where `K'` is the subfilter's internal bit count. The default configuration `filter<T, K>` equals `filter<T, K, block<unsigned char, 1>>`, implementing a classical Bloom filter with `K` bits per insertion.

### Hash Function Quality

The library automatically detects hash function quality using the `boost::hash_is_avalanching` trait. High-quality hash functions pass through unchanged. Lower-quality hash functions receive a bit-mixing post-process that improves their statistical properties. Boost.ContainerHash provides built-in hashing for standard types. Extend `boost::hash` for custom types following the Boost.ContainerHash documentation.

## Subfilters

A subfilter defines how bits are set and checked within each selected subarray. The library provides four subfilter types with different performance and FPR characteristics.

### `block<Block, K'>`

Sets `K'` bits within a single block of type `Block`. The block can be an unsigned integral type (`unsigned char`, `uint32_t`, `uint64_t`) or an array of unsigned integrals (`uint64_t[8]`).

```cpp
// Sets 4 bits in a 64-bit block per subarray
using my_filter = filter<std::string, 2, block<uint64_t, 4>>;
```

**Advantages:** Very fast memory access due to small footprint per operation.

**Disadvantages:** FPR worsens as block size decreases. Smaller blocks concentrate bits, increasing collision probability.

### `multiblock<Block, K'>`

Sets one bit in each of `K'` consecutive blocks of type `Block`. The subarray spans `Block[K']` elements.

```cpp
// Sets 1 bit in each of 8 consecutive 64-bit blocks
using my_filter = filter<std::string, 1, multiblock<uint64_t, 8>>;
```

**Advantages:** Better FPR than `block` for the same block type because bits spread across more memory.

**Disadvantages:** Performance may degrade when subarray spans multiple cache lines.

### `fast_multiblock32<K'>`

Statistically equivalent to `multiblock<uint32_t, K'>` but uses SIMD-accelerated algorithms when SSE2, AVX2, or Neon instructions are available at compile time.

```cpp
// SIMD-accelerated multiblock with 8 bits per subarray
using my_filter = filter<std::string, 1, fast_multiblock32<8>>;
```

**When to use:** Always prefer over `multiblock<uint32_t, K'>` on platforms with SIMD support.

### `fast_multiblock64<K'>`

Statistically equivalent to `multiblock<uint64_t, K'>` but uses SIMD-accelerated algorithms when AVX2 is available.

```cpp
// SIMD-accelerated 64-bit multiblock
using my_filter = filter<std::string, 1, fast_multiblock64<8>>;
```

**When to use:** Prefer over `multiblock<uint64_t, K'>` when AVX2 is available. Provides better FPR than `fast_multiblock32` at the cost of slightly slower operations.

### Subfilter Comparison

| Subfilter | FPR | Speed | Best For |
|-----------|-----|-------|----------|
| `block<Block, K'>` | Higher | Fastest | Speed-critical applications |
| `multiblock<Block, K'>` | Lower | Moderate | Balanced FPR/speed |
| `fast_multiblock32<K'>` | Moderate | Fast | SIMD-enabled systems |
| `fast_multiblock64<K'>` | Lower | Fast | AVX2-enabled systems needing low FPR |

### Stride Parameter

The `Stride` parameter controls the distance in bytes between consecutive subarrays. When set to 0 (the default), stride equals the subarray size, preventing overlap. Setting `Stride` smaller than the subarray size causes subarrays to overlap.

Overlapping improves FPR by 10-40% compared to non-overlapping configurations. The tradeoff is potential performance degradation from unaligned memory access.

```cpp
// Overlapping subarrays with 1-byte stride
using my_filter = filter<std::string, 1, block<uint64_t, 4>, 1>;
```

## Capacity and FPR

### Specifying Capacity Directly

The filter capacity (size in bits) is set at construction time:

```cpp
filter<std::string, 8> f(1000000);  // 1,000,000 bits
std::cout << f.capacity();           // >= 1,000,000
```

The actual capacity may exceed the requested value due to internal alignment requirements. The default constructor creates a filter with zero capacity, which cannot store any elements.

### Calculating Capacity from FPR

When the expected element count and target FPR are known, the library can calculate the required capacity:

```cpp
// Insert 100,000 elements with ~1% FPR
filter<std::string, 8> f(100000, 0.01);

// Equivalent explicit form
filter<std::string, 8> f2(filter<std::string, 8>::capacity_for(100000, 0.01));
```

**Warning:** Very small FPR targets can require enormous capacities. A filter for 100,000 elements with FPR of 1E-50 would require approximately 1.4 terabits, causing `std::bad_alloc`.

### Estimating FPR

The static member function `fpr_for` estimates the false positive rate for a given element count and capacity:

```cpp
filter<std::string, 8> f(1000000);
// Estimate FPR after inserting 10,000 elements
double fpr = filter<std::string, 8>::fpr_for(10000, f.capacity());
```

The filter does not track insertion count. Users must track element counts externally to use `fpr_for`.

### Resizing the Filter

Filters cannot grow dynamically. Use `reset` to change capacity:

```cpp
filter<std::string, 8> f(1000000);
f.insert("hello");

f.reset(2000000);              // New capacity, clears all data
f.reset(50000, 0.01);          // Calculate capacity from n and FPR
f.reset();                     // Zero capacity
```

**Important:** `reset` clears all inserted data. There is no way to resize while preserving contents.

## Operations

### Insertion

Insert elements using `insert`:

```cpp
filter<std::string, 5> f(1000000);

// Single element
f.insert("hello");

// Range of elements
std::vector<std::string> data = {"one", "two", "three"};
f.insert(data.begin(), data.end());

// Initializer list
f.insert({"alpha", "beta", "gamma"});
```

Insertion does not store the element. The operation computes a hash value and sets corresponding bits in the internal array.

### Lookup

Check element membership using `may_contain`:

```cpp
bool found = f.may_contain("hello");  // true if inserted
bool maybe = f.may_contain("world");  // may be true (false positive)
```

The function returns `true` if the element was inserted. The function may also return `true` for elements that were never inserted (false positives). The function never returns `false` for inserted elements (no false negatives).

### Clearing

Remove all elements from the filter:

```cpp
f.clear();  // Sets all bits to zero, capacity unchanged
```

There is no way to remove individual elements. Bloom filters are insert-only data structures.

### Filter Combination

Combine filters using bitwise operators. Both filters must have identical capacity.

**Union (OR):** The result contains elements from both filters.

```cpp
filter<std::string, 5> f1(1000000);
filter<std::string, 5> f2(1000000);

f1.insert("alpha");
f2.insert("beta");

f1 |= f2;  // f1 now "contains" both alpha and beta
```

**Intersection (AND):** The result contains elements present in both filters.

```cpp
f1 &= f2;  // f1 now contains intersection
```

**Caveat:** AND combination produces a filter with higher FPR than a fresh filter containing only the common elements. Do not trust `fpr_for` estimates after AND combination.

## Serialization

Access the raw bit array using `array()` for serialization:

```cpp
#include <fstream>

void save_filter(const filter<std::string, 5>& f, const char* filename)
{
    std::ofstream out(filename, std::ios::binary);
    
    // Save capacity (in bits)
    std::size_t cap = f.capacity();
    out.write(reinterpret_cast<const char*>(&cap), sizeof(cap));
    
    // Save array contents
    auto arr = f.array();
    out.write(reinterpret_cast<const char*>(arr.data()), arr.size());
}

filter<std::string, 5> load_filter(const char* filename)
{
    std::ifstream in(filename, std::ios::binary);
    
    // Read capacity
    std::size_t cap;
    in.read(reinterpret_cast<char*>(&cap), sizeof(cap));
    
    // Create filter and load array
    filter<std::string, 5> f(cap);
    auto arr = f.array();
    in.read(reinterpret_cast<char*>(arr.data()), arr.size());
    
    return f;
}
```

**Important:** The `array()` function returns a span over `unsigned char`. Array size in bytes equals `capacity() / CHAR_BIT`. Filters can only be loaded on systems with matching endianness.

## Choosing a Configuration

Selecting optimal filter parameters requires balancing FPR, memory usage, and performance.

### Classical Filter (Best FPR)

The default configuration provides the lowest FPR for a given bit budget:

```cpp
using my_filter = filter<T, K>;
```

This configuration accesses `K` separate memory locations per operation, resulting in `K` potential cache misses. Performance degrades as `K` increases.

### Block Filter (Best Speed)

Block filters concentrate all bit operations within a small memory region:

```cpp
using my_filter = filter<T, 1, block<uint64_t, K>>;
```

A single 64-bit block fits in one cache access. FPR is higher than the classical filter with the same total bits per operation.

### SIMD-Accelerated Filter (Balanced)

SIMD filters offer a middle ground between FPR and performance:

```cpp
using my_filter = filter<T, 1, fast_multiblock32<K>>;
```

Enable SIMD at compile time:
- GCC/Clang: `-march=native` or `-mavx2`
- MSVC: `/arch:AVX2`

### Optimum K Selection

For a target FPR and known element count, calculate the optimal bits per operation:

1. Determine `c = capacity / n` (bits per element) from FPR charts
2. Calculate `K_opt = c * ln(2) ≈ c * 0.693`
3. Use `floor(K_opt)` or `ceil(K_opt)`

For classical filters with `c = 10` bits per element: `K_opt ≈ 7`.

### Configuration Decision Tree

1. **Need lowest possible FPR?** Use `filter<T, K>` (classical)
2. **Need maximum speed?** Use `filter<T, 1, block<uint64_t, K>>`
3. **Have SIMD support?** Use `filter<T, 1, fast_multiblock32<K>>` or `fast_multiblock64<K>`
4. **Need compromise?** Use `filter<T, 1, multiblock<uint64_t, K>>`

### Practical Example

Configure a filter for 10 million elements with 0.01% FPR:

```cpp
// Step 1: Estimate c from FPR requirement
// For FPR = 0.01% (10^-4), c ≈ 19-22 bits per element depending on filter type

// Step 2: Choose filter type and look up optimal K
// Using fast_multiblock32 with stride=1, c ≈ 21.5, K = 14

// Step 3: Construct filter
using my_filter = filter<std::string, 1, fast_multiblock32<14>, 1>;
my_filter f(10'000'000, 1e-4);  // Let library calculate capacity
```

## Caveats and Limitations

### Thread Safety

The `filter` class provides no thread safety guarantees. Concurrent reads are safe. Concurrent writes or mixed read/write access require external synchronization.

### No Element Removal

Bloom filters cannot remove individual elements. Clearing the entire filter is the only removal operation. Applications requiring deletion should consider alternative data structures such as counting Bloom filters or cuckoo filters.

### FPR Degradation

FPR increases as more elements are inserted. Inserting beyond the planned capacity causes rapid FPR degradation. Monitor insertion counts and resize or recreate filters before reaching capacity.

### Memory Alignment

Some subfilter configurations require aligned memory access for optimal performance. Using `Stride` values that cause misalignment may reduce throughput on certain architectures.

### Endianness

Serialized filters are not portable across systems with different endianness. Filters saved on little-endian systems cannot be loaded on big-endian systems without byte-order conversion.

## API Reference Summary

### Constructors

| Signature | Description |
|-----------|-------------|
| `filter()` | Default constructor, zero capacity |
| `filter(size_t m)` | Capacity of `m` bits |
| `filter(size_t n, double fpr)` | Capacity for `n` elements at target FPR |
| `filter(first, last, size_t m)` | Construct and insert range |
| `filter(first, last, size_t n, double fpr)` | Construct with FPR target and insert range |

### Member Functions

| Function | Description |
|----------|-------------|
| `insert(x)` | Insert element `x` |
| `insert(first, last)` | Insert range of elements |
| `may_contain(x)` | Check if `x` might be present |
| `clear()` | Set all bits to zero |
| `reset(m)` | Change capacity to `m` bits, clear data |
| `reset(n, fpr)` | Change capacity based on `n` and FPR |
| `capacity()` | Return capacity in bits |
| `array()` | Return span over internal array |
| `hash_function()` | Return copy of hash function |
| `get_allocator()` | Return copy of allocator |
| `operator\|=(f)` | Union with filter `f` |
| `operator&=(f)` | Intersection with filter `f` |
| `swap(f)` | Swap contents with filter `f` |

### Static Member Functions

| Function | Description |
|----------|-------------|
| `capacity_for(n, fpr)` | Calculate capacity for `n` elements at FPR |
| `fpr_for(n, m)` | Estimate FPR for `n` elements in capacity `m` |

### Type Aliases

| Alias | Description |
|-------|-------------|
| `value_type` | Element type `T` |
| `hasher` | Hash function type |
| `allocator_type` | Allocator type |
| `size_type` | Unsigned integer type for sizes |
| `subfilter` | Subfilter type |

### Constants

| Constant | Description |
|----------|-------------|
| `k` | Number of subarrays per insertion (`K`) |
| `stride` | Stride between subarrays in bytes |
