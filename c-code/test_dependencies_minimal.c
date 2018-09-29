#include <stdio.h>

int x(void) {
  return 10;
}

void y(int n) {
  int tmp = n+n;
}

void z(void) {
  int tmp = 1;
}

int main(void) {
  int x_ret = x();
  y(x_ret);

  z();
  return 0;
}
