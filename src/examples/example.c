#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cida.h"

int main(int argc, char** argv)
{
  ci_init(&argc, &argv);

  int a = ci_rank( );

  int ids[10] = {0, -1, 2, 4, 10, 1, 30, 5, -2, 6};

  int leader = ci_election(ids[ci_rank( )]);
  printf("Leader: %d\n", leader);

  ci_barrier( );
  ci_finalize( );

  printf("%d finalized\n", a);

  return EXIT_SUCCESS;
}
