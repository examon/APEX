#include <stdio.h>

int x(void) {
  return 10;
}

void y(void) {
}

int main(void) {
  int x_ret = x();
  y();

  return 0;
}
