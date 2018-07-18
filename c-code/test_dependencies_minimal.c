#include <stdio.h>

void x(void);
void y(void);
int a(void);
int b(void);

void m(void);
int n(void);

void i(void);
int j(void);
void k(void);

// first branch
void x(void) {
  printf("in x\n");
  int a_result = a();
  y();
}

void y(void) {
  printf("in y\n");
}

int a(void) {
  printf("in a\n");
  int b_result = b();
  return b_result;
}

int b(void) {
  printf("in b\n");
  return 42;
}

// second branch

void m(void) {
  //printf("in m\n");
  //int n_result = n();
}

int n(void) {
  //printf("in n\n");
  //return 4;
}

int main(void) {
  m();
  x();
  return 0;
}
