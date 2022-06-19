#include <mpi.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "parser.h"
#include "topology.h"

#define SYNC_MASTER 0

#define MARKER      0
#define SNAPSHOTTER 0
#define ALL_IN      20000
#define ALL_OUT     10000

// marker flags
#define RED   0
#define GREEN 1

#define INIT         0
#define EXPLORER_TAG 0
#define ECHOBACK_TAG 1

typedef struct process_state
{
  int state;
  int out;
  int in;
} process_state;

void echo_a(__ci_graph* graph, int initiator)
{
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  int size;
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Status status;

  int count          = 0;
  int dist_from_root = 0;
  int parents[size];
  memset(parents, -1, sizeof(int) * size);
  int n_succs = graph->nodes[rank].successors->size;
  int* succs  = graph->nodes[rank].successors->array;

  int n_preds = graph->nodes[rank].predecessors->size;
  int* preds  = graph->nodes[rank].predecessors->array;

  if ( rank == initiator )
  {
    __ci_print_neighbors(graph, size);
    dist_from_root = 1;

    for ( int i = 0; i < n_succs; ++i )
      MPI_Send(&dist_from_root, 1, MPI_INT, succs[i], 0, MPI_COMM_WORLD);

    int out_buf;
    for ( int i = 0; i < n_preds; ++i )
    {
      MPI_Recv(&out_buf, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG,
               MPI_COMM_WORLD, &status);
    }
  }
  else
  {
    MPI_Recv(&dist_from_root, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG,
             MPI_COMM_WORLD, &status);
    int source    = status.MPI_SOURCE;
    parents[rank] = source;
    int out_buf;

    dist_from_root++;
    for ( int i = 0; i < n_succs; ++i )
      if ( succs[i] != source )
        MPI_Send(&dist_from_root, 1, MPI_INT, succs[i], 0, MPI_COMM_WORLD);

    for ( int i = 0; i < n_preds; ++i )
      if ( preds[i] != source )
        MPI_Recv(&dist_from_root, 1, MPI_INT, preds[i], MPI_ANY_TAG,
                 MPI_COMM_WORLD, &status);
    MPI_Send(&dist_from_root, 1, MPI_INT, source, 0, MPI_COMM_WORLD);
  }

  MPI_Barrier(MPI_COMM_WORLD);
  printf("Rank %d with parent %d\n", rank, parents[rank]);
}

process_state* cl_initiator( )
{
  int size;
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  process_state* global_states = malloc(sizeof(process_state) * size);

  process_state initiator_state = {1, 2, 3};

  global_states[rank] = initiator_state;

  char marker = 'm';

  // send out a marker
  for ( int i = 0; i < size; ++i )
  {
    if ( i != rank ) MPI_Send(&marker, 1, MPI_CHAR, i, 0, MPI_COMM_WORLD);
  }

  // central server that stores the global state
  for ( int i = 0; i < size; ++i )
  {
    if ( i != rank )
      MPI_Recv((global_states + i), 3, MPI_INT, i, 0, MPI_COMM_WORLD,
               MPI_STATUS_IGNORE);
  }

  return global_states;
}

process_state* chandy_lamport2(__ci_graph* graph, int initiator)
{
  int rank;
  int size;
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  srand(time(NULL) + rank);

  if ( rank == initiator ) return cl_initiator( );
  process_state local_state = NULL;
  char marker;

  MPI_Recv(&marker, 1, MPI_CHAR, MPI_ANY_SOURCE, MPI_COMM_WORLD);

  if ( !local_state )
  {
    local_state.state = rand( ) % size + 1;
    local_state.out   = rand( ) % size + 1;
    local_state.in    = rand( ) % size + 1;

    for ( int i = 0; i < size; ++i )
      if ( i != rank && i != initiator )
        // lock send mutex
        MPI_Send(&marker, 1, MPI_CHAR, i, 0, MPI_COMM_WORLD);
  }
  else
  {
  }

  MPI_Send(local_state, 3, MPI_INT, initiator, 0, MPI_COMM_WORLD);

  return NULL;
}
process_state* chandy_lamport(__ci_graph* graph, int initiator)
{
  int rank;
  int size;
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  int markers[size];
  process_state state;
  process_state* global = malloc(sizeof(process_state) * size);

  memset(markers, RED, sizeof(int) * size);

  process_state marker = {-1, -1, -1};

  srand(time(NULL) + rank);

  for ( int i = 0; i < size; ++i )
    if ( rank != i ) MPI_Send(&marker, 3, MPI_INT, i, 0, MPI_COMM_WORLD);

  for ( int i = 0; i < size; ++i )
  {
    if ( rank == i ) continue;

    MPI_Recv(&marker, 3, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    if ( markers[i] == RED )
    {
      state.state = rank;
      state.out   = rand( ) % size;
      state.in    = rand( ) % size;
      markers[i]  = GREEN;
    }
  }

  // send back to initiator
  MPI_Send(&state, 3, MPI_INT, initiator, 0, MPI_COMM_WORLD);

  if ( rank == initiator )
  {
    // store incoming state
    for ( int i = 0; i < size; ++i )
      MPI_Recv((global + i), 3, MPI_INT, i, 0, MPI_COMM_WORLD,
               MPI_STATUS_IGNORE);
    return global;
  }
  else
  {
    printf("r:%d s:%d ", rank, state.out);
    printf("\n");
  }

  return NULL;
}

int main(int argc, char** argv)
{
  MPI_Init(&argc, &argv);

  int size;
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  __ci_file* file   = __ci_read_topology_file("uwg.topo");
  __ci_graph* graph = __ci_parse_file(file, size);

  int i = 0;
  // echo_a(graph, i);
  process_state* states = chandy_lamport2(graph, i);

  if ( states )
  {
    for ( int i = 1; i < size; ++i )
      printf("i:%d, s:%d \n", i, states[i].state);

    printf("\n");
  }

  MPI_Finalize( );

  return EXIT_SUCCESS;
}

