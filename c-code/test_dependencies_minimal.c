#include <stdio.h>

int x(void) {
  return 10;
}

int y(int n) {
  return n;
}

int main(void) {
  int x_ret = x();
  y(x_ret);

  return 0;
}
