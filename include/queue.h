#ifndef __QUEUE_H
#define __QUEUE_H

#include <stddef.h>

typedef struct __ci_queue_node
{
  void *data;
  struct __ci_queue_node *next;
} __ci_queue_node;

typedef struct __ci_queue
{
  int size;
  size_t size_of_type;
  __ci_queue_node *head;
  __ci_queue_node *tail;
} __ci_queue;

void __ci_queue_init(__ci_queue *queue, size_t size_of_type);

int __ci_queue_push(__ci_queue *queue, const void *data);

void __ci_queue_pop(__ci_queue *queue, void *data);

void __ci_queue_peek(__ci_queue *queue, void *data);

void __ci_queue_remove(__ci_queue *queue, void *data);

void __ci_queue_free(__ci_queue *queue);

int __ci_queue_size(__ci_queue *queue);

int __ci_queue_is_empty(__ci_queue *queue);

#endif
