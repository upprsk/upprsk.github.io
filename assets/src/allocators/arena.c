#include "arena.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  arena_t arena = {};
  printf("[0] blocks=%p\n", arena.blocks);

  int *things = arena_alloc(&arena, sizeof(*things), _Alignof(*things));
  printf("[1] blocks=%p, head=%p\n", arena.blocks, arena.blocks->head);

  things = arena_alloc(&arena, sizeof(*things) * 4, _Alignof(*things));
  printf("[2] blocks=%p, head=%p\n", arena.blocks, arena.blocks->head);

  char *big = arena_alloc(&arena, 990, _Alignof(*big));
  printf("[3] blocks=%p, head=%p\n", arena.blocks, arena.blocks->head);

  arena_clear(&arena);

  return EXIT_SUCCESS;
}
