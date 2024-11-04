#include "block_allocator.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct item {
  char name[32];
  int socket;
} item_t;

int main() {
  static uint8_t buffer[4096];

  block_allocator_t alloc;
  ba_init(&alloc, buffer, sizeof(buffer), sizeof(item_t));

  item_t *item0 = ba_alloc(&alloc);
  printf("item0=%p\n", item0);
  // item0=0x58773c9b1020

  item_t *item1 = ba_alloc(&alloc);
  printf("item1=%p\n", item1);
  // item1=0x58773c9b0ffc

  ba_free(&alloc, item0);

  item_t *item2 = ba_alloc(&alloc);
  printf("item0=%p, item2=%p\n", item0, item2);
  // item0=0x58773c9b1020, item2=0x58773c9b1020

  return EXIT_SUCCESS;
}
