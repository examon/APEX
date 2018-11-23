#include <stdio.h>
#include <malloc.h>


int n(void) {
  printf("n: in\n");
  return 10;
}

int z(void) {
  printf("z: in\n");
  int a = 1;
  int b = n();
  return 10;
}

int x(void) {
  printf("x: in\n");
  int test = 1;
  return z();
}

int flag(void) {
  printf("flag: in\n");
  return 0;
}

int main(int argc, char **argv) {
  int f = flag();
  int test = x();

  if (f == 0) {
    printf("if: in\n");
    int x_ret = x();
  } else {
    printf("else: in\n");
    int n_ret = n();
  }

  int z_ret = z();
  return 0;
}
