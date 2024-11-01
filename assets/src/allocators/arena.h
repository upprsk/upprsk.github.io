#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

// Align up the given integer to the given alignment.
#define ALIGN_TO(_value, _alignment)                                           \
  ((_value) + ((_alignment) - 1) & -(_alignment))

typedef struct arena_block {
  struct arena_block *next;
  uint8_t *buffer_end;
  uint8_t *head;
  uint8_t buffer[];
} arena_block_t;

typedef struct arena {
  arena_block_t *blocks;
} arena_t;

#define ARENA_BLOCK_SIZE 1024

static inline void arena_block_init(arena_block_t *b, arena_block_t *next) {
  b->next = next;
  b->buffer_end = ((uint8_t *)b) + ARENA_BLOCK_SIZE;
  b->head = b->buffer;
}

static void *arena_block_alloc(arena_block_t *b, size_t size, size_t align) {
  // get the head, aligned correctly and check if we have enough space
  uint8_t *head = (uint8_t *)ALIGN_TO((uintptr_t)b->head, align);
  if (head + size > b->buffer_end) {
    return NULL;
  }

  // move the head forward and return
  b->head = head + size;

  return head;
}

static arena_block_t *arena_new_block(arena_t *a) {
  arena_block_t *blk = malloc(ARENA_BLOCK_SIZE);
  if (!blk) return NULL;

  // initialize the block and prepend to the linked list
  arena_block_init(blk, a->blocks);
  a->blocks = blk;

  return blk;
}

static arena_block_t *arena_get_block(arena_t *a) {
  if (a->blocks)
    return a->blocks;

  // allocate the first block in the list
  return arena_new_block(a);
}

static void *arena_alloc(arena_t *a, size_t size, size_t align) {
  arena_block_t *blk = arena_get_block(a);
  if (!blk)
    return NULL;

  void *buf = arena_block_alloc(blk, size, align);
  if (buf)
    return buf;

  blk = arena_new_block(a);
  return arena_block_alloc(blk, size, align);
}

static void arena_clear(arena_t *a) {
  arena_block_t *b = a->blocks;
  while (b) {
    arena_block_t *next = b->next;
    free(b);
    b = next;
  }

  a->blocks = NULL;
}
