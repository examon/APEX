#include <stdio.h>

int x(void) {
  return 10;
}

void y(int n) {
  int tmp = n+n;
}

int z(void) {
  int tmp = 1;
  return tmp;
}

int main(void) {
  int x_ret = x();
  y(x_ret);

  int z_ret = z();
  int z_store = z_ret;
  // TODO: Add fcn call to function that takes @z_store.

  return 0;
}
