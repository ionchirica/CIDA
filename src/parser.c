#include "parser.h"

#include <fcntl.h> /* O_RDONLY */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h> /* mmap */
#include <sys/stat.h> /* fstat */
#include <unistd.h>

#include "topology.h"
#include "util.h"

#define SIZE 10

/*
 * Parses a topology file and creates a graph
 * Rings and edges have a capitalized version that make the edges undirected.
 */

/* smelly code fuuuu */
/*                         file contents  number of processors*/
__ci_graph* __ci_parse_file(__ci_file* read_file, int size)
{
  __ci_graph* graph = malloc(sizeof(__ci_graph));
  __ci_init_nodes(graph, size);

  char* delim         = "\n"; /* split by lines */
  char* line          = strtok(read_file->contents, delim);
  char* allowed_specs = "ReriCE";

  /* line by line */
  while ( line != NULL )
  {
    __ci_vector* arguments;
    char line_spec = line[0];

    if ( strchr(allowed_specs, line_spec) )
    {
      char* char_arguments;
      arguments = __ci_vector_init( );

      /* since there are two strtoks, C will lose reference to one of them */
      /* this way, the pointer is not lost */
      char* save_ptr;
      char_arguments = strtok_r(line, " \t", &save_ptr);

      /* argument by argument */
      while ( char_arguments != NULL )
      {
        char* ptr_strtol;
        int int_argument = strtol(char_arguments, &ptr_strtol, 10);
        /* if is a number, store it */
        if ( ptr_strtol > char_arguments && int_argument < size )
          __ci_vector_push_back(arguments, int_argument);

        /* its a wildcard */
        else if ( !strcmp(char_arguments, "*") )
        {
          /* start at the previous read element or make all */
          int i =
              arguments->size ? arguments->array[arguments->size - 1] + 1 : 0;
          for ( ; i < size; ++i ) __ci_vector_push_back(arguments, i);
        }

        char_arguments = strtok_r(NULL, " \t", &save_ptr);
      }

      switch ( line_spec )
      {
        case 'e':
          __ci_directed_edge(graph, arguments->array, arguments->size);
          break;
        case 'E':
          __ci_undirected_edge(graph, arguments->array, arguments->size);
          break;
        case 'r':
          __ci_directed_ring(graph, arguments->array, arguments->size);
          break;
        case 'R':
          __ci_undirected_ring(graph, arguments->array, arguments->size);
          break;
        case 'C':
          __ci_clique(graph, arguments->array, arguments->size);
          break;
        case 'i':
          __ci_set_initiator(graph, arguments->array, arguments->size);
          break;
        default:
          break;
      }

      free(char_arguments);
      __ci_vector_free(arguments);
    }

    line = strtok(NULL, delim);
  }

  return graph;
}

/*
 * Maps a virtual page with mmap
 * and returns the struct file { contents, size }
 * */
__ci_file* __ci_read_topology_file(const char* filename)
{
  __ci_file* read_file;
  char* contents;
  struct stat s;
  int file_descriptor;
  int size;

  read_file = malloc(sizeof(__ci_file));

  if ( !read_file ) __ci_error("Could not allocate memory\n");

  file_descriptor = open(filename, O_RDONLY);

  if ( file_descriptor == -1 )
  {
    free(read_file);
    __ci_error("Could not open file\n");
  }

  fstat(file_descriptor, &s);

  size = s.st_size;

  contents =
      mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, file_descriptor, 0);

  if ( !contents )
  {
    free(read_file);

    __ci_error("Could not map the file\n");
  }

  read_file->contents = contents;
  read_file->size     = size;

  close(file_descriptor);
  return read_file;
}

