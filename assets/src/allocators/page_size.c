#include <stdio.h>
#include <unistd.h>

int main(void) {
  long page_size = sysconf(_SC_PAGESIZE);

  printf("page_size=%ld", page_size);
}
