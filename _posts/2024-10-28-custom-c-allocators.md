---
title: "Custom Allocators in C"
date: 2024-10-28
categories:
  - blog
tags:
  - allocators
  - c
classes: wide
---

Memory allocation is not fun, specially in C. Even if the lack of fat pointers is
ignored, every single allocation needs to be kept in my small brain in order to be
cleaned-up later. And don't forget, only a single `free` can bring balance to the
program, no more, no less.

No wonder languages with _garbage collectors_ are so popular. Makes it all so easy.

There are times however that a GC is definitely an overkill, or even simply impossible.
But why would that be?

First, sometimes your language does not support one (like C), and it is not viable
to change to a language that has one. A GC brings a runtime with it, and most of
the time it is not small. If you are developing for WASM or embedded, code size
is important and needs to be taken into account.

The second reason, and often the one that prevails, is _speed_.

In applications that require high performance, having a GC pausing the code to
run will ruin your day. Think of a game engine, where you are fighting for that
frame rate, or a trading system, where being slower will lose you money. Memory
allocation is not fast, even when manual, so controlling _when_ it happens becomes
the deciding factor.

But then are we going back to `malloc` and `free`? Of course not, there are much
better ways to handle allocation, depending on when and how you intend to use the
memory. This is the realm of _custom build allocators_.

## Linear allocators

This is as simple as an allocator can get. You have a buffer of bytes and each
allocation request is just an offset into it.

```c
#define BUFFER_SIZE 1024
uint8_t buffer[BUFFER_SIZE];
size_t  head = 0;

void* allocate(size_t size) {
  void* mem = &buffer[head];
  head += size;
  if (head > BUFFER_SIZE) {
    return NULL;
  }

  return mem;
}
```

And that's it. When `allocate()` is called, we return a chunk of memory and
increment past it for the next call. If there is not enough space in the buffer,
we return `NULL`.

We can then use our freshly baked allocator like this:

```c
int main(void) {
  int *data = allocate(12 * sizeof(*data));
  if (!data) {
    perror("allocate");
    return EXIT_FAILURE;
  }

  printf("buffer=%p, data=%p, head=%zu\n", buffer, data, head);
  // stdout: buffer=0x6468babc4040, data=0x6468babc4040, head=48

  data = allocate(32 * sizeof(*data));
  if (!data) {
    perror("allocate");
    return EXIT_FAILURE;
  }

  printf("buffer=%p, data=%p, head=%zu\n", buffer, data, head);
  // stdout: buffer=0x6468babc4040, data=0x6468babc4070, head=176

  return EXIT_SUCCESS;
}
```

In this example, the memory looks like this:

![Linear allocation](/assets/images/linear_allocator.jpg){: style="width: 50%;"}

Now you might ask: where is the free? And this is where the first big difference
from the standard allocators enters the scene. **There is no free**. Or at least
not one for each of the allocations.

You see, the linear allocator is exactly that: linear. Allocations are done
sequentially without holes and _zero_ metadata. That means we have no way of
knowing where each allocation starts and ends, and no way of creating holes for reuse.

"Then what is the use for this allocator?" you ask. Well, there is a pattern that
most allocations share: they are done in groups. Most of the time we don't have single
allocations with individual lifetimes, we have many allocations that _share a single
lifetime_. Consider an HTTP server, it needs to perform many allocations during the
request lifecycle, but all of them are bound by a single lifetime: the request.
In a game engine, many allocations are performed, but a considerable amount will
not be used past the end of the frame.

A linear allocator takes advantage of that and groups all allocations done through
it to a single lifetime. Then, when it expires, we free all the allocations in
sweet **constant time**:

```c
void reset(void) {
  head = 0;
}
```

By setting a single variable, the entire buffer is free again. This not only fast
for the computer, but also reduces the mental burden placed upon the developer.

Now, we shall name this a **Fixed Buffer Allocator** (or FBA) before giving it a
proper implementation, because yes, although the above works, it has some big flaws.
Big enough to make it unusable.

### Memory Alignment

Computers have a harsh limitation on how memory can be accessed: the address of
the accessed memory needs to be aligned to its integer size.

But what does that mean?

When the processor requests to read or write an integer of some size, the address
where it is placed needs to be a multiple of its size. So a 4 byte integer can only
be loaded or stored in an address that is a multiple of 4, a 2 byte integer can only
be loaded or stored in an even address, and so on and so forth.

![Memory alignment](/assets/images/memory_alignment.jpg){: style="width: 50%;"}

This means that our current implementation, simply bumping the head forward (this
is where the other name for this allocator, **Bump Allocator**, comes from) will
produce unaligned allocations:

```c
short* a = allocate(sizeof(*a));
int*   b = allocate(sizeof(*b)); // unaligned!
```

To fix this, we need to round up our allocation head to the next multiple of the
desired alignment.

> View [this](https://stackoverflow.com/a/4840428) stack overflow answer (to a
> completely unrelated question) for a good explanation of how the rounding
> happens. It explains the topic better than me.

```c
#define ALIGN_TO(_value, _alignment) \
  ((_value) + ((_alignment) - 1) & -(_alignment))
```

The macro above will perform the alignment for us, so that `ALIGN_TO(2, 4)` will
be 4, `ALIGN_TO(9, 8)` will be 16, etc.

### A proper implementation

Armed with such knowledge, we can create a complete implementation for a very
simple, very fast, fixed buffer allocator:

```c
// fba.h
#include <stddef.h>
#include <stdint.h>

// Align up the given integer to the given alignment.
#define ALIGN_TO(_value, _alignment)                                           \
  ((_value) + ((_alignment) - 1) & -(_alignment))

// A Fixed Buffer Allocator.
//
// We store a buffer (start and end) and the current allocation head.
typedef struct fba {
  uint8_t *buffer;
  uint8_t *buffer_end;
  uint8_t *head;
} fba_t;

// Initialize the fba to a given buffer.
static inline void fba_init(fba_t *fba, uint8_t *buffer, size_t buffer_size) {
  *fba = (fba_t){
      .buffer = buffer, .buffer_end = buffer + buffer_size, .head = buffer};
}

// Clear all allocations in the buffer.
static inline void fba_reset(fba_t *fba) { fba->head = fba->buffer; }

// Allocate `size` bytes with `align` alignment.
static inline void *fba_alloc_opt(fba_t *fba, size_t size, size_t align) {
  // get the head, aligned correctly and check if we have enough space
  uint8_t *head = (uint8_t *)ALIGN_TO((uintptr_t)fba->head, align);
  if (head + size > fba->buffer_end) {
    return NULL;
  }

  // move the head forward and return
  fba->head = head + size;

  return head;
}

// Allocate `size` bytes with alignment large enough for any value.
static inline void *fba_alloc(fba_t *fba, size_t size) {
  return fba_alloc_opt(fba, size, _Alignof(void *));
}
```

You will notice that the implementation changed slightly from the first explanation.
The size was changed to a pointer to the end of the buffer and `head` also became
pointer. You should not be afraid of pointers, as most of the time they make the
code more concise (and faster as a bonus).

In my opinion, storing the buffer start and end is easier to understand, as we
are dealing with a memory region and don't really use it as an array in any point.
When allocating, we check if the allocation head, now also a pointer, is located
in the region.

![FBA struct](/assets/images/fba.jpg){: style="width: 50%;"}

But finally, we can now use our allocator:

```c
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "fba.h"

#define BUFFER_SIZE 1024

int main() {
  // create a static buffer for our allocations. It is important to say that if
  // we wanted, this buffer could be dynamically allocated by another allocator
  // (like `malloc`). We just need the bytes, it does not matter where they came
  // from.
  static uint8_t buffer[BUFFER_SIZE];

  // create an instance of our allocator. As it's state is contained in the
  // struct, we could have as many of these as we want.
  fba_t fba = {};
  fba_init(&fba, buffer, BUFFER_SIZE);

  // allocate an integer
  int *v = fba_alloc_opt(&fba, sizeof(*v), _Alignof(*v));
  if (!v) {
    perror("fba_alloc_opt");
    return EXIT_FAILURE;
  }

  // allocate a struct
  struct {
    int a;
    int b;
  } *s = fba_alloc_opt(&fba, sizeof(*s), _Alignof(*s));

  // allocate a string
  char *str = fba_alloc_opt(&fba, 32, _Alignof(char));

  // clear all allocations. v, s and str now point to invalid memory
  fba_reset(&fba);

  return EXIT_SUCCESS;
}
```

We can even have some fun with it, ever wanted easy strings in C?

```c
static char *fba_vsprintf(fba_t *fba, char const *format, va_list args) {
  va_list args2;
  va_copy(args2, args);

  int n = vsnprintf(NULL, 0, format, args);
  char *buf = fba_alloc_opt(fba, n + 1, _Alignof(char));
  if (buf) {
    vsnprintf(buf, n + 1, format, args2);
  }

  va_end(args2);

  return buf;
}

static char *fba_sprintf(fba_t *fba, char const *format, ...) {
  va_list args;
  va_start(args, format);
  char *s = fba_vsprintf(fba, format, args);
  va_end(args);

  return s;
}

int main() {
  char *hello = fba_sprintf(&fba, "Hello, %s!", "world");
  if (!hello) {
    perror("fba_alloc_opt");
    return EXIT_FAILURE;
  }

  printf("%s\n", hello);
  // stdout: Hello, World!

  return EXIT_SUCCESS;
}
```

There is, however, an issue. We need to know what will be the peek memory usage
beforehand, as the size of the buffer is fixed. When we desire to cap memory
usage (like in an embedded device) this is excellent, but it is not very good in
a more general use case. This is where a more advanced linear allocator is required.

## Arena Allocators
