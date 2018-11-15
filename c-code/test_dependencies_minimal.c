#include <stdio.h>
#include <malloc.h>

int global_1 = 10;



int y (int n) {
  int a = n+n;
  return a;
}

int j(int n) {
  int a = n+n;
  return a;
}

int x(void) {
  int j_ret = j(1);
  return j_ret;
}

int z(void) {
  printf("z: in\n");
  int tmp = 1;
  printf("z: out\n");
  return tmp;
}

int n(void) {
  printf("n: in\n");
  int y_ret = y(10);
  printf("n: out\n");
  return y_ret;
}

int flag(void) {
  return 0;
}

int main(int argc, char **argv) {
  int f = flag();

  if (f == 0) {
    int x_ret = x();
    int y_ret = y(x_ret);
  } else {
    int n_ret = n();
    int n_store = n_ret;
  }
  return 0;
}
