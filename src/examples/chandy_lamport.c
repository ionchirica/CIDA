#include <mpi.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "list.h"
#include "parser.h"
#include "topology.h"

#define SEND    0
#define RECEIVE 1

#define RED   0
#define GREEN 1

#define INITIATOR 0

#define MARKER  0
#define MESSAGE 1

typedef struct message
{
  int type;
  int data;
  int source;
  int sink;
} message;

MPI_Datatype msg_struct;

typedef struct List* queue;

typedef struct process
{
  int rank;
  int state;
  int source;
  int* markers;
  bool recorded;

  queue incoming_queue;
} process;

process local_proc;

void take_snapshot(__ci_node* node, int source)
{
  if ( local_proc.recorded == false )
  {
    local_proc.recorded = true;

    int* succ   = node->successors->array;
    int n_succs = node->successors->size;

    char marker = 'm';

    for ( int i = 0; i < n_succs; ++i )
      if ( succ[i] != source )
        MPI_Send(&marker, 1, MPI_CHAR, succ[i], 0, MPI_COMM_WORLD);

    local_proc.state = rand( ) % 1000;
    printf("local_proc.state %d\n", local_proc.state);
  }
}

bool all_green(__ci_node* node)
{
  int n_preds = node->predecessors->size;

  int greens = 0;
  for ( int i = 0; i < n_preds; ++i )
    if ( local_proc.markers[i] == GREEN ) greens++;

  return (greens - 1) == n_preds;
}

int find_adj(int j, __ci_node* node)
{
  int* preds  = node->predecessors->array;
  int n_preds = node->predecessors->size;

  for ( int i = 0; i < n_preds; ++i )
    if ( preds[i] == j ) return i;

  return -1;
}

void receive(int size, int rank, __ci_node* node)
{
  MPI_Status status;
  char marker;

  int* succ   = node->successors->array;
  int n_succs = node->successors->size;
  int* preds  = node->predecessors->array;
  int n_preds = node->predecessors->s;
  ze;

  while ( !all_green(node) )
  {
    MPI_Recv(&marker, 1, MPI_CHAR, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
    if ( !local_proc.recorded )
    {
      take_snapshot(node, status.MPI_SOURCE);

      local_proc.markers[find_adj(status.MPI_SOURCE, node)] = GREEN;
    }
    // was already marked, enqueue the message
    else
    {
      message msg = {MESSAGE, rand( ) % size, status.MPI_SOURCE, rank};
      pushList(local_proc.incoming_queue, &msg);
    }
  }

  MPI_Barrier(MPI_COMM_WORLD);

  if ( rank != INITIATOR )
    MPI_Send(&local_proc.state, 1, MPI_INT, INITIATOR, 0, MPI_COMM_WORLD);

  printf("l: %d r:%d\n", local_proc.state, rank);
}

int main(int argc, char** argv)
{
  MPI_Init(&argc, &argv);

  int size;
  int rank;
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  __ci_file* file   = __ci_read_topology_file("uwg.topo");
  __ci_graph* graph = __ci_parse_file(file, size);

  __ci_node* node = &graph->nodes[rank];

  local_proc.rank = rank;

  local_proc.incoming_queue = makeList( );
  local_proc.markers        = malloc(sizeof(int) * node->predecessors->size);
  local_proc.recorded       = false;

  memset(local_proc.markers, RED, sizeof(int) * node->predecessors->size);

  int* global_state = malloc(sizeof(int) * size);

  srand(time(NULL) + rank);

  if ( rank == INITIATOR )
  {
    __ci_print_neighbors(graph, size);
    take_snapshot(node, -1);
  }
  receive(size, rank, node);

  if ( rank == INITIATOR )
  {
    for ( int i = 0; i < size; ++i )
      if ( rank != i )
        MPI_Recv((global_state + i), 1, MPI_INT, i, 0, MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);

    for ( int i = 0; i < size - 1; ++i )
    {
      printf("%d ", global_state[i]);
    }
    printf("\n");
  }

  MPI_Finalize( );
  return EXIT_SUCCESS;
}
