#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct block_allocator_block {
  struct block_allocator_block *next;
} block_allocator_block_t;

typedef struct block_allocator {
  uint8_t *buffer;
  uint8_t *buffer_end;
  block_allocator_block_t *blocks;
} block_allocator_t;

static void ba_init(block_allocator_t *ba, uint8_t *buffer, size_t buffer_size,
                    size_t item_size) {
  assert(item_size >= sizeof(block_allocator_block_t));
  size_t item_count = buffer_size / item_size;

  // initialize all of the blocks, this creates a linked list of free blocks
  block_allocator_block_t *prev = NULL;
  for (size_t i = 0; i < item_count; i++) {
    block_allocator_block_t *b =
        (block_allocator_block_t *)(buffer + i * item_size);
    *b = (block_allocator_block_t){.next = prev};

    prev = b;
  }

  ba->buffer = buffer;
  ba->buffer_end = buffer + buffer_size;
  ba->blocks = prev;
}

static void *ba_alloc(block_allocator_t *ba) {
  if (!ba->blocks)
    return NULL;

  block_allocator_block_t *blk = ba->blocks;
  ba->blocks = blk->next;

  // clear at least the reference to the next block before returning
  memset(blk, 0, sizeof(block_allocator_block_t));
  return blk;
}

static void ba_free(block_allocator_t *ba, void *ptr) {
  uint8_t *p = ptr;

  // don't put in our list pointers that are not in our buffer
  if (p < ba->buffer || p > ba->buffer_end)
    return;

  block_allocator_block_t *blk = ptr;
  *blk = (block_allocator_block_t){.next = ba->blocks};
  ba->blocks = blk;
}
