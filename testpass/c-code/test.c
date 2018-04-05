#include <stdio.h>

void x(void) {
  printf("in x\n");
}

int n(void) {
  return 1;
}

int z(void) {
  return 100;
}

int y(void) {
  int c = z();
  return c;
}



int main() {
  x();
  int r = y();
  printf("RESULT: %d\n<<<<<<\n", r);
  n();
  return r;
}

/*

 main -> x
      -> y -> z
      -> n

 target z, after pass should be

 main -> y -> z

 */