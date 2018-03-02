void x(void) {
  int i = 0;
}

void n(void) {
  int n = 1;
}

int z(void) {
  return 100;
}

int y(void) {
  int c = z();
  return c;
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