#include <stdio.h>


int x(void) {
  printf("x: in\n");
  printf("x: out\n");
  return 10;
}

int y (int n) {
  return n;
}
//int y(int n) {
//  printf("y: in\n");
//  int tmp = n+n;
//  printf("y: out\n");
//  return tmp;
//}

//int z(void) {
//  printf("z: in\n");
//  int tmp = 1;
//  printf("z: out\n");
//  return tmp;
//}

int n(void) {
  printf("n: in\n");
  int y_ret = y(10);
  printf("n: out\n");
  return y_ret;
}

int main(void) {
  printf("main: in\n");
  int x_ret = x();

  int y_ret = y(x_ret);
  int y_store = y_ret;

//  int z_ret = z();
//  int z_store = z_ret;

  int n_ret = n();
  int n_store = n_ret;

  printf("main: out\n");
  return 0;
}
