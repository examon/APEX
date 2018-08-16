#include <stdio.h>

int x(void) {
  return 10;
}

int y(int n) {
  return n+1;
}

int z(int n) {
  return n+2;
}

int main(void) {
  int x_ret = x();
  y(x_ret);
  z(x_ret);
  
  return 0;
}
