corosio key insight:
coroutine frame allocation cannot be avoided for i/o patterns
the allocation we cannot avoid, pays for the type-erasure we need
the lifetime of parameters passed to your coroutine extends until you finish
type-erasure at the coroutine boundary is thus essentially free (you already paid for the frame)
7:15
Asio cannot do this, because it was not written coroutines-first
7:16
so in asio you end up with stupid shit like any_buffers
7:16
in corosio, your buffer sequence stays in its natural type until the end where it is efficiently dissected into a platform struct
7:16
in asio, you use any_executor and that gets passed through all the layers. if your executor is big, you pay
7:16
in corosio, your executor no matter how complex (sizeof can be 500 bytes), it only costs 2 pointers
7:17
in asio your completion handler grows with each layer of abstraction, struct within struct within struct. the compiler sees through the whole thing, yes, but those memmove of the ever-larger structs add up. (edited) 
7:17
in corosio, the "completion handler" is only ever one pointer (std::coroutine_handler) and each layer of abstraction adds just one more pointer
7:18
5 layers of asio handlers could be 500 bytes, in corosio it is 40 bytes
7:18
furthermore in asio, the i/o operation is templated on all that bullshit, creating NxM combinatorial explosion of both size and code
7:19
in corosio, i/o objects operation sizes are known at compile time, not visible outside the TU, and preallocated. no need for a recycling ops allocator
7:19
now all I need to do is talk about corosio's "negative overhead"
pdimov
  7:40 PM
JUCB
Vinnie
  7:48 PM
JUCB.
7:49
complex refactoring, handled
http does not depend on boost.buffers, fix cmake and jamfiles, change all includes to corresponding capy buffers includes, rename symbols if needed, compile and fix until successful
7:49
merging buffers to capy was the right move
7:49
Oh, here's the cool thing
7:50
when I finish this, it will show how the C++ committee has a path to standardizing the coroutines-first execution model optimized for networking (capy) and they can leave the networking piece to third parties, sidestepping all the bickering and gnashing of design teeth
7:50
with this, there's no need to standardize corosio
7:50
you can write for capy and then just link in your favorite lib
7:51
ABI-stable by design