#include <stdio.h>

int x(void) {
  return 10;
}

int y(int n) {
  return n;
}

int z(void) {
  return 20;
}

int main(void) {
  int x_ret = x();
  y(x_ret);

  int z_ret = z();
  return 0;
}
