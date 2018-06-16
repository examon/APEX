#include <stdio.h>

void x(void);
void y(void);
void a(void);
void b(void);

void x(void) {
  printf("in x\n");
  y();
}

void y(void) {
  printf("in y\n");
}

void a(void) {
  printf("in z\n");
  b();
}

void b(void) {
  printf("in z\n");
}

int main() {
  x();
  a();
}
