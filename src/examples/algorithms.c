#include <mpi.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cida.h"

#define NANO_SLEEP(S, NS) \
  nanosleep((const struct timespec[]){{(S), (NS)}}, NULL)
#define MS (1000000)

typedef enum
{
  WHITE,
  GREEN,
  RED
} colors;

typedef struct proc
{
  bool recorded;
  bool* marked;
  int state;
} proc;

void take_snapshot(proc* p)
{
  int* succs  = ci_get_successors( );
  int n_succs = ci_get_number_successors( );
  char marker = 'c';
  if ( p->recorded == false )
  {
    p->recorded = true;
    for ( int i = 0; i < n_succs; ++i )
    {
      ci_send(&marker, 1, ci_char, succs[i], 0, UNDEF);
      // printf("%d sent %d\n", ci_rank( ), succs[i]);
    }
    p->state = rand( ) % 100;
  }
}

bool all_done(bool* markers, int size_markers)
{
  bool all_true[size_markers];
  memset(all_true, 1, size_markers);
  return !memcmp(all_true, markers, size_markers);
}

int snapshot(int initiator)
{
  int rank = ci_rank( );

  int* preds  = ci_get_predecessors( );
  int n_preds = ci_get_number_predecessors( );

  proc* p     = malloc(sizeof(proc));
  p->recorded = false;
  p->marked   = malloc(sizeof(bool) * n_preds);
  memset(p->marked, 0, sizeof(bool) * n_preds);

  char marker = 'c';

  if ( rank == initiator )
  {
    take_snapshot(p);
    p->state = rand( ) % 100;
  }

  ci_status status;
  bool flag = false;
  while ( !flag )
  {
    ci_recv(&marker, 1, ci_char, CI_ANY_SOURCE, CI_ANY_TAG, &status);
    // printf("%d received %d\n", rank, status.ci_source);

    take_snapshot(p);

    for ( int i = 0; i < n_preds; ++i )
      if ( status.ci_source == preds[i] ) p->marked[i] = true;

    flag = all_done(p->marked, n_preds);
    printf("%d %d\n", rank, all_done(p->marked, n_preds));
  }

  // printf("%d all done\n", rank);

  free(p->marked);
  free(p);

  return EXIT_SUCCESS;
}

int echo(int initiator)
{
  int rank = ci_rank( );

  int* succs  = ci_get_successors( );
  int n_succs = ci_get_number_successors( );
  int n_preds = ci_get_number_predecessors( );

  char marker  = 'm';
  int parent   = -1;
  int received = 0;
  ci_status status;

  if ( rank == initiator )
    for ( int i = 0; i < n_succs; ++i )
      ci_send(&marker, 1, ci_char, succs[i], 0, UNDEF);

  while ( received != n_preds )
  {
    // we should also check if the source is a valid neighbor
    ci_recv(&marker, 1, ci_char, CI_ANY_SOURCE, CI_ANY_TAG, &status);

    received++;
    if ( rank != initiator && parent == -1 )
    {
      parent = status.ci_source;

      if ( n_succs > 1 )
      {
        for ( int i = 0; i < n_succs; ++i )
        {
          if ( succs[i] != status.ci_source )
            ci_send(&marker, 1, ci_char, succs[i], 0, UNDEF);
        }
      }
      else
        ci_send(&marker, 1, ci_char, status.ci_source, 0, UNDEF);
    }
    else if ( received == n_preds )
    {
      if ( parent != -1 ) ci_send(&marker, 1, ci_char, parent, 0, UNDEF);
      parent = status.ci_source;

      break;
    }
    NANO_SLEEP(0, 5 * MS);
  }

  NANO_SLEEP(3, 0);
  ci_barrier( );
  return parent;
}

void do_something( )
{
  int rank = ci_rank( );
  char a   = 'a';

  if ( rank == 1 )
    for ( int i = 0; i < 7; ++i )
    {
      ci_send(&a, 1, ci_char, 2, 0, SYNC);

      NANO_SLEEP(0, 100000L);
    }
  if ( rank == 2 )
  {
    for ( int i = 0; i < 8; ++i )
    {
      ci_recv(&a, 1, ci_char, 1, 0, NULL);
      NANO_SLEEP(0, 100000L);
    }
  }

  NANO_SLEEP(2, 0);
  ci_barrier( );
}

int main(int argc, char** argv)
{
  ci_init(&argc, &argv);

  if ( ci_rank( ) == 0 ) printf("---------------------\n");

  // echo(0);
  do_something( );

  ci_finalize( );

  return EXIT_SUCCESS;
}

