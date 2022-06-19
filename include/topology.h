#ifndef __TOPOLOGY_H
#define __TOPOLOGY_H

#include <stdarg.h>
#include <stdbool.h>

#include "vector.h"

typedef enum
{
  undirected,
  directed
} connection;

typedef struct __ci_node
{
  int rank;
  int parent;

  __ci_vector* predecessors;
  __ci_vector* successors;

  bool is_initiator;

  int state;
} __ci_node;

typedef struct __ci_graph
{
  __ci_node* nodes;
  int num_nodes;

  int* initiators;
  int num_initiators;
} __ci_graph;

void __ci_set_initiator(__ci_graph* graph,
                        int* initiators,
                        int size_initiators);

void __ci_clique(__ci_graph* graph, int* edges, int size_edges);

void __ci_undirected_edge(__ci_graph* graph, int* edges, int size_edges);

void __ci_directed_edge(__ci_graph* graph, int* edges, int size_edges);

void __ci_undirected_ring(__ci_graph* graph, int* edges, int size_edges);

void __ci_directed_ring(__ci_graph* graph, int* edges, int size_edges);

void __ci_ring(__ci_graph* graph,
               int* edges,
               int size_edges,
               connection edge_connection);

bool __ci_are_neighbors(__ci_graph* graph, int source, int sink);

void __ci_init_nodes(__ci_graph* graph, int num_nodes);

void __ci_free_nodes(__ci_graph* graph, int num_nodes);

void __ci_print_neighbors(__ci_graph* graph, int num_nodes);

#endif
