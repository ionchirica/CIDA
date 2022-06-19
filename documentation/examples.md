### Examples
This page is reserved for some program examples that might show to use CIDA.
#### Communication between 2 processors
The topology file, should at least, have an edge with source in 0 and sink in 1.

```
e 0 1
```

Example of a communication between 2 processors. Process 0 sends a payload to processor 1.
```C
#include "../include/cida.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
  ci_init(&argc, &argv);

  // rank of the current process
  int rank = ci_rank();

  // get the number of processes
  int size = ci_size();

  if( size != 2 )
  {
    fprintf(stderr, "This application is meant to be run with 2 processes.\n");
    return EXIT_FAILURE;
  }

  // buffer to send
  char buffer[20] = "Hello from CIDA";

  enum roles = { SENDER, RECEIVER };

  switch ( rank )
  {
    // perform the send operation
    case SENDER:
      ci_send(&buffer, 20, ci_char, 1, 0, SYNC);
      printf("Process %d sent a message to %d\n", rank, 1);

    // perform the receive operation
    case RECEIVER :
      ci_recv(&buffer, 20, ci_char, 1, CI_ANY_TAG, NULL);
      printf("Process %d received \"%s\"\n", buffer);
  }

  // shutdown the MPI library and clean up 
  ci_finalize();

  return EXIT_SUCCESS;
}
```
