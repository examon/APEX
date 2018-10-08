#include <stdio.h>

int x(void) {
  return 10;
}

int y(int n) {
  int tmp = n+n;
  return tmp;
}

int z(void) {
  int tmp = 1;
  return tmp;
}

int main(void) {
  int x_ret = x();

  int y_ret = y(x_ret);
  int y_store = y_ret;

  int z_ret = z();
  int z_store = z_ret;
  // TODO: Add fcn call to function that takes @z_store.

  return 0;
}
