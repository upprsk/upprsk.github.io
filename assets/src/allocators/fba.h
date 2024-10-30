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
