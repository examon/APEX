#include <stdio.h>

int x(void);
int y(void);
void yy(void);
void z(void);
void a(void);
void b(void);

int x(void) {
  printf("in x\n");
  return 666;
}

int y(void) {
  printf("in y\n");
  int yy_result = 42;
}

void yy(void) {
  printf("in yy\n");
}

void z(void) {
  printf("in z\n");
}

void a(void) {
  printf("in a\n");
  b();
  int y_result = y();
  int x_result = x();
  printf("in a: y_result = %d, x_result = %d\n", y_result, x_result);
}

void b(void) {
  printf("in b\n");
  z();
}

int main(void) {
  a();
  return 0;
}
