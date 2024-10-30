#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "fba.h"

#define BUFFER_SIZE 1024

static char *fba_sprintf(fba_t *fba, char const *format, ...);

int main() {
  // create a static buffer for our allocations. It is important to say that if
  // we wanted, this buffer could be dynamically allocated by another allocator
  // (like `malloc`). We just need the bytes, it does not matter where they came
  // from.
  static uint8_t buffer[BUFFER_SIZE];

  // create an instance of our allocator
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
  if (!s) {
    perror("fba_alloc_opt");
    return EXIT_FAILURE;
  }

  // allocate a string
  char *str = fba_alloc_opt(&fba, 32, _Alignof(char));
  if (!str) {
    perror("fba_alloc_opt");
    return EXIT_FAILURE;
  }

  // clear all allocations. v, s and str now point to invalid memory
  fba_reset(&fba);

  char *hello = fba_sprintf(&fba, "Hello, %s", "world");
  if (!hello) {
    perror("fba_alloc_opt");
    return EXIT_FAILURE;
  }

  printf("%s\n", hello);

  return EXIT_SUCCESS;
}

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
