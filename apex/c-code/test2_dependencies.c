#include <stdio.h>

//void x(void);
int y(void);
void a(void);
void b(void);

//void x(void) {
//  printf("in x\n");
//  y();
//}

int y(void) {
  printf("in y\n");
  return 42;
}

void a(void) {
  printf("in a\n");
  b();
  int y_result = y();
  printf("in a: y_result = %d\n", y_result);
}

void b(void) {
  printf("in b\n");
}

int main(void) {
  a();
//  x();
  return 0;
}
