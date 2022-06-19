#ifndef __PARSER_H
#define __PARSER_H

#define DEFAULT_FILENAME "../topologies/default"

typedef struct __ci_graph __ci_graph;
typedef struct __ci_file __ci_file;

typedef struct __ci_file
{
  char* contents; /* actual contents */
  int size;       /* size in bytes   */
} __ci_file;

__ci_file* __ci_read_topology_file(const char* filename);

__ci_graph* __ci_parse_file(__ci_file* file, int size);

#endif
