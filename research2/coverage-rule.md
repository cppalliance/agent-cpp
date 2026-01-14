# C++ Style: One Statement Per Line

For coverage accuracy and breakpoint placement, put everything on its own line:

```cpp
// Braces on own lines, even for empty bodies
void await_resume() const noexcept
{
}

// Parameters, return type, body all separate
async_read_some_t
async_read_some()
{
    return async_read_some_t(*this);
}

// Constructor with initializer list
async_read_some_t(
    socket& s)
    : s_(s)
{
}

// Conditionals: each operand on own line
if( a ||
    b &&
    c)
```

**Why:** Each line gets independent coverage status and can have a breakpoint set on it.
