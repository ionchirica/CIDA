#include "queue.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void __ci_queue_init(__ci_queue *queue, size_t size_of_type)
{
  queue->size         = 0;
  queue->size_of_type = size_of_type;
  queue->head = queue->tail = NULL;
}

int __ci_queue_push(__ci_queue *queue, const void *data)
{
  __ci_queue_node *node = (__ci_queue_node *) malloc(sizeof(__ci_queue_node));

  if ( node == NULL )
  {
    return EXIT_FAILURE;
  }

  node->data = malloc(queue->size_of_type);

  if ( node->data == NULL )
  {
    free(node);
    return EXIT_FAILURE;
  }

  node->next = NULL;

  memcpy(node->data, data, queue->size_of_type);

  if ( queue->size == 0 )
  {
    queue->head = queue->tail = node;
  }
  else
  {
    queue->tail->next = node;
    queue->tail       = node;
  }

  queue->size++;
  return EXIT_SUCCESS;
}

void __ci_queue_pop(__ci_queue *queue, void *data)
{
  if ( queue->size > 0 )
  {
    __ci_queue_node *temp = queue->head;
    memcpy(data, temp->data, queue->size_of_type);

    if ( queue->size > 1 )
    {
      queue->head = queue->head->next;
    }
    else
    {
      queue->head = NULL;
      queue->tail = NULL;
    }

    queue->size--;
    free(temp->data);
    free(temp);
  }
}

void __ci_queue_peek(__ci_queue *queue, void *data)
{
  if ( queue->size > 0 )
  {
    __ci_queue_node *temp = queue->head;
    memcpy(data, temp->data, queue->size_of_type);
  }
}

void __ci_queue_remove(__ci_queue *queue, void *data)
{
  __ci_queue_node *tmp  = queue->head;
  __ci_queue_node *prev = queue->head;

  while ( tmp )
  {
    if ( memcmp(data, tmp->data, queue->size_of_type) == 0 )
    {
      if ( tmp == prev )
      {
        queue->head = tmp->next;
        if ( !tmp->next ) queue->tail = tmp;
      }
      else
      {
        prev->next = tmp->next;
        if ( !prev->next ) queue->tail = prev;
      }

      free(tmp);
      queue->size--;
      break;
    }

    if ( prev != tmp ) prev = prev->next;
    tmp = tmp->next;
  }
}

void __ci_queue_free(__ci_queue *queue)
{
  __ci_queue_node *temp;

  while ( queue->size > 0 )
  {
    temp        = queue->head;
    queue->head = temp->next;
    free(temp->data);
    free(temp);
    queue->size--;
  }

  queue->head = queue->tail = NULL;
}

int __ci_queue_size(__ci_queue *queue) { return queue->size; }

int __ci_queue_is_empty(__ci_queue *queue) { return queue->size == 0; }
