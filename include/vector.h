#ifndef __VECTOR_H
#define __VECTOR_H

#include <stdbool.h>
#include <stdlib.h>

#define INITIAL_CAPACITY ((int8_t) 5)
#define GROWTH_FACTOR	 ((int8_t) 2)

typedef struct
{
  int* array;
  size_t size;
  size_t capacity;
} __ci_vector;

/*
 * Initializes a vector of integers
 * capacity = INITIAL_CAPACITY
 * size = 0
 *
 * A vector is a data structure that allows:
 *    O(1) amortized insert.
 *    O(1) search.
 *    Delete is not implemented since it will be not needed.
 *
 * @returns new vector or null
 * */
__ci_vector* __ci_vector_init( );

/*
 * Push back means it will add to the end of the array.
 * If there is not enough space to insert an item,
 * the vector will grow according to the GROWTH_FACTOR.
 *
 * @returns EXIT_SUCCESS or EXIT_FAILURE
 * */
int __ci_vector_push_back(__ci_vector* vector, int item);

/*
 * Removes element at index
 *
 * @returns the element
 * */
int __ci_vector_remove_at(__ci_vector* vector, int index);

void __ci_vector_print(__ci_vector* vector);

/*
 * Frees the memory used.
 * */
void __ci_vector_free(__ci_vector* vector);

bool __ci_vector_is_empty(__ci_vector* vector);

int __ci_vector_size(__ci_vector* vector);

#endif
