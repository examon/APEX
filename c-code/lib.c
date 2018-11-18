#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/// Function used for getting out of the program, usually after calling @lib_dump_int().
void lib_exit(int exit_code) {
  exit(exit_code);
}

/// Function used for dumping value @i to file @APEX_OUTPUT.
void lib_dump_int(int i) {
#define APEX_OUTPUT "apex.out"
  // Figure out number of digits @i has.
  // So we can allocate big enough buffer.
  int i_digits = 0;
  if (i == 0) {
    i_digits = 1;
  } else {
    i_digits = floor(log10(abs(i)))+1;
  }
  // Store @i as string into @i_string buffer.
  char i_string[i_digits+2];
  sprintf(i_string, "%d\n", i);

  // Save @i_string into @APEX_OUTPUT
  int status = 0;
  FILE *f = fopen(APEX_OUTPUT, "w+");
  if (f != NULL) {
    if (fputs(i_string, f) != EOF) {
      status = 1;
    }
    fclose(f);
  }
  if (status != 1) {
    printf("Error: Could not save to file!\n");
  }
}