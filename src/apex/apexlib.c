#include <stdio.h>
#include <stdlib.h>

/// Function used for getting out of the program, usually after calling @_apex_extract_int().
void _apex_exit(int exit_code) {
  exit(exit_code);
}

/// Function used for extracting value %i.
void _apex_extract_int(int i) {
  printf("%d", i);
}
