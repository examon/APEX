int x(void) {
  return 1;
}

int z(void) {
  return 100;
}

int y(void) {
  int c = z();
  return c;
}

int n(void) {
  return 2;
}

int main() {
  x();
  int r = y();
  n();
  return r;
}

/*

 main -> x
      -> y -> z
      -> n

 target z, after pass should be

 main -> y -> z

 */