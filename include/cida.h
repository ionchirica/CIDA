#ifndef __CIDA_H
#define __CIDA_H
#include <mpi.h>

#include "topology.h"
//..^

/*
 *TODO:
  send, recv
  mensagens de acordo com o garfo
  filosofos
  algoritmo de controlo
 * */

/*
 * No real way to prevent user from doing some sketchy stuff
 * ie: ci_char | ci_int & ci_long
 * */
// struct {.ci , .mpi}
// buffer source []
// necessidade de manter registo de mensagens enviadas
typedef enum
{
  ci_char,
  ci_short,
  ci_int,
  ci_long,
  ci_long_long_int,
  ci_long_long,
  ci_signed_char,
  ci_unsigned_char,
  ci_unsigned_short,
  ci_unsigned,
  ci_unsigned_long,
  ci_unsigned_long_long,
  ci_float,
  ci_double,
  ci_long_double,
  ci_wchar
} datatypes;

typedef enum
{
  SYNC,
  UNDEF
} communication_mode;

#define CI_ANY_SOURCE ((char) -1)
#define CI_ANY_TAG    ((char) -1)

#define CI_EXIT_SUCCESS 0
#define CI_EXIT_FAILURE 1

typedef void* generic;

typedef datatypes ci_datatype;

typedef MPI_Request ci_request;

typedef struct ci_status
{
  int ci_tag;
  int ci_source;
} ci_status;

int ci_init(int* argc, char*** argv);

int ci_send(generic buffer,
            int count,
            ci_datatype datatype,
            int receiver,
            int tag,
            communication_mode mode);

int ci_isend(generic buffer,
             int count,
             ci_datatype datatype,
             int receiver,
             int tag,
             ci_request* request);

int ci_recv(generic buffer,
            int count,
            ci_datatype datatype,
            int sender,
            int tag,
            ci_status* status);

int ci_irecv(generic buffer,
             int count,
             ci_datatype datatype,
             int sender,
             int tag,
             ci_request* status);

int ci_wait(ci_request* request, ci_status* status);

int ci_waitall(int count, ci_request request[], ci_status status[]);

int ci_size( );

int ci_rank( );

int ci_barrier( );

int ci_probe(int source, int tag, ci_status* status);

int ci_iprobe(int sender, int tag, int* flag, ci_status* status);

int* ci_get_predecessors( );

int* ci_get_successors( );

int ci_get_number_predecessors( );

int ci_get_number_successors( );

int ci_election(int unique_id);

void ci_print_graph( );

int ci_finalize( );

#endif
