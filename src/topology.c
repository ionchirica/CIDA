#include "topology.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

void __ci_set_initiator(__ci_graph* graph, int* initiators, int size_initiators)
{
  assert(size_initiators >= 1);

  for ( int i = 0; i < size_initiators; ++i )
    graph->nodes[initiators[i]].is_initiator = true;

  graph->num_initiators = size_initiators;
  graph->initiators     = malloc(sizeof(int) * graph->num_initiators);
  memcpy(graph->initiators, initiators, size_initiators * sizeof(int));
}

void __ci_clique(__ci_graph* graph, int* edges, int size_edges)
{
  assert(size_edges >= 2);

  /* is it range defined or array defined ? */
  bool size_flag = size_edges == 2;

  int start = size_flag ? edges[0] : 0;
  int end   = size_flag ? edges[1] : size_edges;

  for ( int i = start; i < end; ++i )
    for ( int j = i + 1; j < end; ++j )
    {
      int pair[2];
      pair[0] = size_flag ? i : edges[i];
      pair[1] = size_flag ? j : edges[j];

      __ci_undirected_edge(graph, pair, 2);
    }
}

bool __ci_are_neighbors(__ci_graph* graph, int source, int sink)
{
  for ( int i = 0; i < graph->nodes[source].successors->size; ++i )
    if ( graph->nodes[source].successors->array[i] == sink ) return true;
  return false;
}

void __ci_undirected_edge(__ci_graph* graph, int* edges, int size_edges)
{
  assert(size_edges >= 2);
  int head = edges[0];
  for ( int i = 1; i < size_edges; ++i )
  {
    if ( !__ci_are_neighbors(graph, edges[i], head) )
    {
      __ci_vector_push_back(graph->nodes[edges[i]].successors, head);
      __ci_vector_push_back(graph->nodes[edges[i]].predecessors, head);

      __ci_vector_push_back(graph->nodes[head].successors, edges[i]);
      __ci_vector_push_back(graph->nodes[head].predecessors, edges[i]);
    }
  }
}

void __ci_directed_edge(__ci_graph* graph, int* edges, int size_edges)
{
  assert(size_edges >= 2);
  int head = edges[0];
  for ( int i = 1; i < size_edges; ++i )
  {
    if ( !__ci_are_neighbors(graph, edges[i], head) )
    {
      __ci_vector_push_back(graph->nodes[head].successors, edges[i]);

      __ci_vector_push_back(graph->nodes[head].predecessors, edges[i]);
    }
  }
}

void __ci_ring(__ci_graph* graph,
               int* edges,
               int size_edges,
               connection edge_connection)
{
  assert(size_edges >= 2);

  int head = edges[0];
  int tail = edges[size_edges - 1];

  int next = head + 1;
  /* ranged__ci_ring */
  if ( size_edges == 2 )
  {
    for ( int i = head; i <= tail; ++i, next = i + 1 )
    {
      if ( i == tail ) next = head;

      int pair[2] = {i, next};

      edge_connection == directed ? __ci_directed_edge(graph, pair, 2)
                                  : __ci_undirected_edge(graph, pair, 2);
    }
  }
  /* array defined__ci_ring */
  else
  {
    for ( int i = 0; i < size_edges - 1; i++ )
    {
      int pair[2] = {edges[i], edges[i + 1]};

      edge_connection == directed ? __ci_directed_edge(graph, pair, 2)
                                  : __ci_undirected_edge(graph, pair, 2);
    }

    int ring_ends[2] = {tail, head};
    /* z <-> a , connect head and tail */
    edge_connection == directed ? __ci_directed_edge(graph, ring_ends, 2)
                                : __ci_undirected_edge(graph, ring_ends, 2);
  }
}

/*
 * For a undirected__ci_ring from x0 to xy
 * xy x1, x0 x2, ..., xy-1 x0
 * Property:
 * In an undirected graph, a node its connected to two others,
 * the predecessor and the sucessor
 * So for any Xy node, its connected to Xy+1 and Xy-1
 * */
void __ci_undirected_ring(__ci_graph* graph, int* edges, int size_edges)
{
  __ci_ring(graph, edges, size_edges, undirected);
}

void __ci_directed_ring(__ci_graph* graph, int* edges, int size_edges)
{
  __ci_ring(graph, edges, size_edges, directed);
}

void __ci_init_nodes(__ci_graph* graph, int num_nodes)
{
  graph->nodes      = malloc(sizeof(__ci_node) * num_nodes);
  graph->initiators = NULL;
  for ( int i = 0; i < num_nodes; ++i )
  {
    graph->nodes[i].rank         = i;
    graph->nodes[i].successors   = __ci_vector_init( );
    graph->nodes[i].predecessors = __ci_vector_init( );
    graph->nodes[i].is_initiator = false;

    if ( !graph->nodes[i].predecessors || !graph->nodes[i].successors )
      __ci_error("Unable to initialize a vector.\n");
  }
}

void __ci_free_nodes(__ci_graph* graph, int num_nodes)
{
  for ( int i = 0; i < num_nodes; ++i )
  {
    __ci_vector_free(graph->nodes[i].predecessors);
    __ci_vector_free(graph->nodes[i].successors);
  }

  if ( graph->initiators ) free(graph->initiators);
  free(graph->nodes);
  free(graph);
}

void __ci_print_neighbors(__ci_graph* graph, int num_nodes)
{
  printf("------------------\n");
  printf("Neighbors \n");
  for ( int i = 0; i < num_nodes; ++i )
  {
    int num_neighbors = graph->nodes[i].successors->size;

    if ( num_neighbors ) printf("Rank: %d\n Neighbors: ", i);
    for ( int j = 0; j < num_neighbors; ++j )
    {
      printf("%d ", graph->nodes[i].successors->array[j]);
    }

    if ( num_neighbors ) printf("\n");
  }
  printf("------------------\n");
}
