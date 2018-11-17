#include <stdio.h>
#include <malloc.h>

int global_1 = 10;

int y (int n) {
  printf("y: in\n");
  int a = n+n;
  return a;
}

int j(int n) {
  printf("j: in\n");
  int a = n+n;
  return a;
}

int x(void) {
  printf("x: in\n");
  int j_ret = j(1);
  return j_ret;
}

int z(void) {
  printf("z: in\n");
  int tmp = 1;
  return tmp;
}

int n(void) {
  printf("n: in\n");
  int y_ret = y(10);
  return y_ret;
}

int flag(void) {
  printf("flag: in\n");
  return 0;
}

int main(int argc, char **argv) {
  int f = flag();

  if (f == 0) {
    printf("if: in\n");
    int x_ret = x();
    int y_ret = y(x_ret);
  } else {
    printf("else: in\n");
    int n_ret = n();
    int n_store = n_ret;
  }

  int z_ret = z();
  return 0;
}
