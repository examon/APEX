#include <stdio.h>
#include <stdlib.h>

void test(void) {
  return;
}

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

int n(void) {
  int y_ret = y(10);
  return y_ret;
}

int main(void) {
  int x_ret = x();

  int y_ret = y(x_ret);
  int y_store = y_ret;

  int z_ret = z();
  int z_store = z_ret;

  int n_ret = n();
  int n_store = n_ret;

  return 0;
}
