#include <stdio.h>

int x(void);
int y(void);
int a(void);
int b(void);

int x(void) {
  printf("x\n");
  y();
}

int y(void) {
  printf("y\n");
}

int a(void) {
  printf("a\n");
  y();
  b();
}

int b(void) {
  printf("b\n");
}

int main() {
  x();
  a();

  return 0;
}
