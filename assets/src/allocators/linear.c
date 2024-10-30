#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE 1024
uint8_t buffer[BUFFER_SIZE];
size_t head = 0;

void *allocate(size_t size) {
  void *mem = &buffer[head];
  head += size;
  if (head > BUFFER_SIZE) {
    return NULL;
  }

  return mem;
}

int main(void) {
  int *data = allocate(12 * sizeof(*data));
  if (!data) {
    perror("allocate");
    return EXIT_FAILURE;
  }

  printf("buffer=%p, data=%p, head=%zu\n", buffer, data, head);

  data = allocate(32 * sizeof(*data));
  if (!data) {
    perror("allocate");
    return EXIT_FAILURE;
  }

  printf("buffer=%p, data=%p, head=%zu\n", buffer, data, head);

  return EXIT_SUCCESS;
}
