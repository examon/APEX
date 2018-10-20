#include <stdio.h>
#include <stdlib.h>

void lib_test(void)
{
    printf("lib_test() from lib.c\n");
}

void lib_exit(int exit_code) {
  exit(exit_code);
}

