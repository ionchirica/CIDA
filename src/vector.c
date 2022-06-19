#include "vector.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Initialise an empty __ci_vector */
__ci_vector* __ci_vector_init( )
{
  __ci_vector* vector;
  int* vector_pointer;

  vector         = malloc(sizeof(__ci_vector));
  vector_pointer = malloc(sizeof(int) * INITIAL_CAPACITY);

  if ( vector_pointer == NULL )
  {
    fprintf(stderr, "Unable to allocate memory, exiting.\n");
    free(vector_pointer);
    return NULL;
  }

  vector->array    = vector_pointer;
  vector->size     = 0;
  vector->capacity = INITIAL_CAPACITY;

  return vector;
}

int __ci_vector_push_back(__ci_vector* vector, int item)
{
  int* vector_pointer;

  if ( vector->size + 1 == vector->capacity )
  {
    vector->capacity *= GROWTH_FACTOR;

    vector_pointer = realloc(vector->array, vector->capacity * sizeof(int));

    /* realloc failed... */
    if ( vector_pointer == NULL )
    {
      fprintf(stderr, "Unable to reallocate memory, exiting.\n");
      free(vector_pointer);
      return EXIT_FAILURE;
    }

    vector->array = vector_pointer;
  }

  /* push back item */
  vector->array[vector->size++] = item;

  return EXIT_SUCCESS;
}

int __ci_vector_remove_at(__ci_vector* vector, int index)
{
  // allocate an array with a size 1 less than the current one
  int* new_array = malloc((vector->size - 1) * sizeof(int));
  int element    = vector->array[index];

  // copy everything BEFORE the index
  if ( index != 0 ) memcpy(new_array, vector->array, index * sizeof(int));

  if ( index != (vector->size - 1) )
    memcpy(new_array + index, vector->array + index + 1,
           (vector->size - index - 1) * sizeof(int));

  vector->size--;
  vector->array = new_array;
  return element;
}

void __ci_vector_print(__ci_vector* vector)
{
  printf("__ci_vector: ");
  for ( int i = 0; i < vector->size; ++i )
    printf(i == vector->size - 1 ? "%d" : "%d ", vector->array[i]);

  printf("\n");
}

/* free __ci_vector */
void __ci_vector_free(__ci_vector* vector)
{
  free(vector->array);
  free(vector);
}

bool __ci_vector_is_empty(__ci_vector* vector) { return vector->size == 0; }

int __ci_vector_size(__ci_vector* vector) { return vector->size; }

