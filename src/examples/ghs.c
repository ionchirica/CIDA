#include "ghs.h"

#include <mpi.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "list.h"
#include "parser.h"
#include "topology.h"

#define INF 1000000

// PROCESS THE MESSAGE LATER?
// HAVE A QUEUE OF INCOMING MESSAGES AND
// PUSHBACK

typedef struct payload
{
  int source;
  int sink;
  int level;
  int weight;
  int state_of_node;
  int fragment;
  message op;

} payload;
MPI_Datatype msg_struct;

node* local_node;
List* message_queue;

bool run = true;

int find_min_edge( )
{
  int min_ind = -1;
  int min     = INF;
  for ( int i = 0; i < local_node->num_edges; ++i )
  {
    if ( min > local_node->edges[i].weight )
    {
      min     = local_node->edges[i].weight;
      min_ind = i;
    }
  }

  return min_ind;
}

void init(int num_edges, int edges[], int weights[], node* nodes)
{
  message_queue           = makeList( );
  local_node->num_edges   = num_edges;
  local_node->edges       = malloc(sizeof(edge) * num_edges);
  local_node->edge_states = malloc(sizeof(int) * num_edges);

  for ( int i = 0; i < num_edges; ++i )
  {
    local_node->edges[i].node   = edges[i];
    local_node->edges[i].weight = weights[i];
    local_node->edge_states[i]  = E_BASIC;
  }

  local_node->state       = SLEEPING;
  local_node->best_edge   = -1;
  local_node->best_weight = INF;
  local_node->test_edge   = -1;
  local_node->parent      = -1;
  local_node->level       = -1;
  local_node->find_count  = -1;
  local_node->frag        = INF;
}

payload* saved_message;
void read_message( )
{
  while ( run )
  {
    payload* message = malloc(sizeof(payload));

    MPI_Status status;
    MPI_Recv(message, 1, msg_struct, MPI_ANY_SOURCE, MPI_ANY_TAG,
             MPI_COMM_WORLD, &status);

    saved_message = message;

    int j = status.MPI_SOURCE;

    if ( local_node->state == SLEEPING ) wake_up( );

    switch ( message->op )
    {
      case CONNECT:
        _connect(message->level, j);
        break;
      case INITIATE:
        initiate(message->level, message->fragment, message->state_of_node, j);
        break;
      case TEST_MSG:
        test_message(message->level, message->fragment, j);
        break;
      case ACCEPT:
        accept(j);
        break;
      case REJECT:
        reject(j);
        break;
      case REPORT_MSG:
        report_message(message->weight, j);
        break;
      case CHANGE_ROOT:
        change_root(j);
        break;
      case WAKEUP:
        wake_up( );
        break;
    }
  }
}

void wake_up( )
{
  int min_edge = find_min_edge( );

  local_node->edge_states[min_edge] = E_BRANCH;
  local_node->level                 = 0;
  local_node->state                 = FOUND;
  local_node->find_count            = 0;

  printf("%d wakeup min_edge %d weight %d\n", local_node->ind, min_edge,
         local_node->edges[min_edge].weight);

  payload msg = {
      .level = 0, .op = CONNECT, .sink = local_node->edges[min_edge].node};

  /* Connect(0) to min_edge */
  MPI_Send(&msg, 1, msg_struct, local_node->edges[min_edge].node, CONNECT,
           MPI_COMM_WORLD);
}

void reject(int j)
{
  printf("%d reject \n", local_node->frag);

  if ( local_node->edge_states[j] == E_BASIC )
    local_node->edge_states[j] = E_REJECT;

  test( );
}

edge find_edge(int e)
{
  for ( int i = 0; i < local_node->num_edges; ++i )
    if ( local_node->edges[i].node == e ) return local_node->edges[i];
}

void accept(int j)
{
  printf("%d accept \n", local_node->ind);
  local_node->test_edge = -1;

  for ( int i = 0; i < local_node->num_edges; ++i )
  {
    if ( local_node->edges[i].node == j )
    {
      if ( local_node->edges[j].weight < local_node->best_weight )
      {
        local_node->best_edge   = j;
        local_node->best_weight = local_node->edges[j].weight;
      }
      break;
    }
  }

  report( );
}

void report( )
{
  printf("%d report \n", local_node->ind);
  int k = 0;
  for ( int i = 0; i < local_node->num_edges; i++ )
  {
    if ( local_node->edge_states[i] == E_BRANCH &&
         local_node->edges[i].node != local_node->parent )
      k++;
  }

  local_node->state = FOUND;
  if ( local_node->find_count == k && local_node->test_edge == -1 )
  {
    payload msg = {.level  = local_node->best_weight,
                   .op     = REPORT_MSG,
                   .source = local_node->parent};
    MPI_Send(&msg, 1, msg_struct, local_node->parent, REPORT_MSG,
             MPI_COMM_WORLD);
  }
}

void _connect(int level, int j)
{
  printf("%d connect \n", local_node->ind);

  edge ej = find_edge(j);

  if ( local_node->state == SLEEPING ) wake_up( );

  if ( level < local_node->level )
  {
    local_node->edge_states[j] = E_BRANCH;
    payload msg                = {.level         = local_node->level,
                                  .fragment      = local_node->frag,
                                  .state_of_node = local_node->state,
                                  .op            = INITIATE,
                                  .sink          = ej.node};

    MPI_Send(&msg, 1, msg_struct, ej.node, INITIATE, MPI_COMM_WORLD);

    if ( local_node->state = FIND ) local_node->find_count++;
  }
  else if ( local_node->edge_states[j] == E_BASIC )
  {
    pushList(message_queue, saved_message);
  }
  else
  {
    payload msg = {.level         = local_node->level + 1,
                   .state_of_node = FIND,
                   .fragment      = ej.weight,
                   .op            = INITIATE,
                   .sink          = ej.node};
    MPI_Send(&msg, 1, msg_struct, ej.node, INITIATE, MPI_COMM_WORLD);
  }
}

void initiate(int level, int fragment, int state_of_node, int j)
{
  printf("%d initiate \n", local_node->ind);
  local_node->level       = level;
  local_node->frag        = fragment;
  local_node->state       = state_of_node;
  local_node->parent      = local_node->edges[j].node;
  local_node->best_edge   = -1;
  local_node->best_weight = INF;

  for ( int i = 0; i < local_node->num_edges; ++i )
  {
    if ( local_node->edges[i].node != local_node->edges[j].node &&
         local_node->edge_states[i] == E_BRANCH )
    {
      payload msg = {.level         = level,
                     .fragment      = fragment,
                     .state_of_node = state_of_node,
                     .op            = INITIATE,
                     .source        = local_node->ind,
                     .sink          = local_node->edges[i].node};

      MPI_Send(&msg, 1, msg_struct, local_node->edges[i].node, INITIATE,
               MPI_COMM_WORLD);
      if ( state_of_node == FIND ) local_node->find_count++;
    }
  }

  if ( state_of_node == FIND )
  {
    local_node->find_count = 0;
    test( );
  }
}

void test( )
{
  printf("%d test \n", local_node->ind);

  int min_ind = -1;
  int min     = INF;
  for ( int i = 0; i < local_node->num_edges; ++i )
  {
    if ( local_node->edge_states[min_ind] == E_BASIC )
      if ( min > local_node->edges[i].weight )
      {
        min     = local_node->edges[i].weight;
        min_ind = i;
      }
  }

  if ( min_ind >= 0 )
  {
    local_node->test_edge = min_ind;
    payload msg           = {.level    = local_node->level,
                             .fragment = local_node->frag,
                             .op       = TEST_MSG,
                             .sink     = local_node->edges[min_ind].node};
    MPI_Send(&msg, 1, msg_struct, local_node->edges[min_ind].node, TEST_MSG,
             MPI_COMM_WORLD);
  }
  else
  {
    local_node->test_edge = -1;
    report( );
  }
}

void test_message(int level, int fragment, int j)
{
  edge ej = find_edge(j);
  printf("%d test message j:%d ej:%d\n", local_node->ind, j, ej.node);

  if ( level > local_node->level )
  {
    /*
    payload msg = {.level    = level,
                   .fragment = fragment,
                   .op       = TEST_MSG,
                   .sink     = local_node->ind};
    MPI_Send(&msg, 1, msg_struct, local_node->ind, TEST_MSG, MPI_COMM_WORLD);
    */

    pushList(message_queue, saved_message);
  }
  else if ( fragment != local_node->frag )
  {
    payload msg = {.op = ACCEPT, .sink = local_node->ind};
    MPI_Send(&msg, 1, msg_struct, ej.node, ACCEPT, MPI_COMM_WORLD);
  }
  else
  {
    if ( local_node->edge_states[j] == E_BASIC )
    {
      local_node->edge_states[j] = E_REJECT;
      if ( j != local_node->test_edge )
      {
        payload msg = {.op = REJECT, .sink = local_node->ind};

        MPI_Send(&msg, 1, msg_struct, ej.node, REJECT, MPI_COMM_WORLD);
      }
      else
        test( );
    }
  }
}

void report_message(int weight, int j)
{
  printf("%d report message\n", local_node->ind);
  if ( j != local_node->parent )
  {
    local_node->find_count++;
    if ( weight < local_node->best_weight )
    {
      local_node->best_edge   = j;
      local_node->best_weight = weight;
    }
    report( );
  }
  else
  {
    if ( local_node->state == FIND )
    {
      payload msg = {.weight = weight, .op = TEST_MSG, .sink = j};
      MPI_Send(&msg, 1, msg_struct, local_node->ind, REPORT_MSG,
               MPI_COMM_WORLD);
    }
    else if ( weight > local_node->best_weight )
    {
      change_root( );
    }
    else if ( weight == INF && local_node->best_weight == INF )
    {
      run = false;
    }
  }
}

void change_root( )
{
  printf("%d change root\n", local_node->ind);
  if ( local_node->edge_states[local_node->best_edge] == E_BRANCH )
  {
    payload msg = {.op   = CHANGE_ROOT,
                   .sink = local_node->edges[local_node->best_edge].node};
    MPI_Send(&msg, 1, msg_struct, local_node->edges[local_node->best_edge].node,
             CHANGE_ROOT, MPI_COMM_WORLD);
  }
  else
  {
    payload msg = {.level = local_node->level,
                   .op    = CONNECT,
                   .sink  = local_node->edges[local_node->best_edge].node};
    MPI_Send(&msg, 1, msg_struct, local_node->edges[local_node->best_edge].node,
             CONNECT, MPI_COMM_WORLD);
    local_node->edge_states[local_node->best_edge] = E_BRANCH;
  }
}

void print_top(node* nodes, int num_nodes)
{
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  printf("------------------------\n");
  for ( int j = 0; j < local_node->num_edges; ++j )
  {
    printf("n:%d e:%d w:%d\n", rank, local_node->edges[j].node,
           local_node->edges[j].weight);
  }

  printf("------------------------\n");
}

int* weigh_graph(__ci_graph* graph, int size)
{
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  __ci_vector preds   = *graph->nodes[rank].successors;
  int* weighted_edges = malloc(sizeof(int) * preds.size);
  srand(time(NULL) + rank);
  for ( int i = 0; i < preds.size; ++i )
  {
    weighted_edges[i] = rand( ) % size + 1;
  }
  return weighted_edges;
}

int main(int argc, char** argv)
{
  MPI_Init(&argc, &argv);
  int rank;
  int size;

  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  int block_lengths[7]  = {1, 1, 1, 1, 1, 1, 1};
  MPI_Datatype types[7] = {MPI_INT, MPI_INT, MPI_INT, MPI_INT,
                           MPI_INT, MPI_INT, MPI_INT};
  MPI_Aint offsets[7];
  offsets[0] = offsetof(payload, source);
  offsets[1] = offsetof(payload, sink);
  offsets[2] = offsetof(payload, level);
  offsets[3] = offsetof(payload, weight);
  offsets[4] = offsetof(payload, fragment);
  offsets[5] = offsetof(payload, state_of_node);
  offsets[6] = offsetof(payload, op);
  MPI_Type_create_struct(7, block_lengths, offsets, types, &msg_struct);
  MPI_Type_commit(&msg_struct);

  node* nodes = malloc(sizeof(node) * size);
  local_node  = malloc(sizeof(node));
  local_node  = &nodes[rank];

  nodes[rank].ind = rank;

  __ci_file* file   = __ci_read_topology_file("uwg.topo");
  __ci_graph* graph = __ci_parse_file(file, size);

  // weigh the edges with random values
  int* weighted_edges = weigh_graph(graph, size);

  for ( int i = 0; i < graph->nodes[rank].successors->size; ++i )
  {
    printf("r:%d e:%d w:%d\n", rank, graph->nodes[rank].successors->array[i],
           weighted_edges[i]);
  }

  init(graph->nodes[rank].successors->size,
       graph->nodes[rank].successors->array, weighted_edges, nodes);

  MPI_Barrier(MPI_COMM_WORLD);

  if ( rank == 0 ) wake_up( );
  read_message( );

  MPI_Finalize( );

  return EXIT_SUCCESS;
}

