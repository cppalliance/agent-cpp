# Javadocs

Boost C++ Javadoc style. Brief on first line after `/**`. Functions returning values: brief starts with "Return".

**Tags:** `@param`, `@return`, `@pre`, `@post`, `@throws`, `@note`, `@see`, `@code`/`@endcode`

**@tparam:** Use for template params, except variadic (`Args...`) or types deduced from function params.

### Example

```cpp
/** Return the size of the buffer sequence.
    @param buffers The buffer sequence to measure.
    @return The total byte count.
*/
template<class BufferSequence>
std::size_t buffer_size(BufferSequence const& buffers);
// No @tparam—BufferSequence evident from @param

/** Return the default value.
    @tparam T The value type.
*/
template<class T>
T default_value();
// @tparam needed—T has no corresponding param
```
