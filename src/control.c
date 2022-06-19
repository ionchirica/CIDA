#include "control.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cida.h"
#include "util.h"

__ci_process_state* __ci_state;
/*
 * Returns the index of the adjacency
 * */
int __ci_adj_to_ind(int process_to_find,
                    int* target_array,
                    int size_target_array)
{
  for ( int i = 0; i < size_target_array; i++ )
    if ( target_array[i] == process_to_find ) return i;
  return -1;
}

/*--------------> Echo <------------*/
/*
 * Echo algorithm that allows the
 * mapping of an undirected defined
 * graph.
 * Returns the parent of the process.
 *
 *----------------------------------*/
int __ci_algorithm_echo(int initiator, int tag, MPI_Comm* communicator)
{
  int rank = ci_rank( );

  int* successors    = ci_get_successors( );
  int n_successors   = ci_get_number_successors( );
  int n_predecessors = ci_get_number_predecessors( );

  char marker  = 'm';
  int parent   = -1;
  int received = 0;
  MPI_Status status;

  if ( rank == initiator )
    for ( int i = 0; i < n_successors; ++i )
      MPI_Send(&marker, 0, MPI_CHAR, successors[i], tag, *communicator);

  while ( received != n_predecessors )
  {
    MPI_Recv(&marker, 0, MPI_CHAR, MPI_ANY_SOURCE, tag, *communicator, &status);

    received++;
    if ( rank != initiator && parent == -1 )
    {
      parent = status.MPI_SOURCE;

      if ( n_successors > 1 )
      {
        for ( int i = 0; i < n_successors; ++i )
          if ( successors[i] != status.MPI_SOURCE )
            MPI_Send(&marker, 0, MPI_CHAR, successors[i], tag, *communicator);
      }
      else
        MPI_Send(&marker, 0, MPI_CHAR, status.MPI_SOURCE, tag, *communicator);
    }
    else if ( received == n_predecessors )
    {
      if ( parent != -1 )
        MPI_Send(&marker, 0, MPI_CHAR, parent, tag, *communicator);
      else
        // echo finished, decide what to do
        parent = status.MPI_SOURCE;
    }
  }

  MPI_Barrier(*communicator);
  return parent;
}

/*-------> Snapshot - Take snapshot<---------*/
/*
 * Performs the procedure Take snapshot of the
 * Chandy-Lamport algorithm.
 * If the process's state has not been recorded,
 * send out markers to all outgoing channels
 * and locally save its state.
 *
 *-------------------------------------------*/

void __ci_algo_take_snapshot(__ci_snapshot_process* process,
                             MPI_Comm* communicator)
{
  int* successors  = ci_get_successors( );
  int n_successors = ci_get_number_successors( );
  char marker      = 'm';

  if ( process->recorded == false )
  {
    process->recorded = true;
    for ( int i = 0; i < n_successors; ++i )
      MPI_Send(&marker, 1, MPI_CHAR, successors[i], __CI_SNAPSHOT_TAG,
               *communicator);

    // save state
    process->state = __ci_state;
  }
}

/*-------> Snapshot - All done <---------*/
/*
 * Compare the array of marked incoming channels
 * with an auxiliary array full of ones (true)
 *
 *---------------------------------------*/

bool __ci_algo_snapshot_all_done(bool* markers, int size_markers)
{
  // new array with all 1's
  bool all_true[size_markers];
  memset(all_true, 1, size_markers);

  // compare all_true with markers
  return !(memcmp(all_true, markers, size_markers));
}

/*--------------> Snapshot <------------*/
/*
 * Algorithm that snapshots the local
 * states of the processes and the
 * messages in transit.
 *
 *--------------------------------------*/

__ci_snapshot_process* __ci_algorithm_snapshot(int initiator,
                                               MPI_Comm* communicator)
{
  int rank = ci_rank( );

  int* predecessors = ci_get_predecessors( );

  int n_predecessors = ci_get_number_predecessors( );

  __ci_snapshot_process* p = malloc(sizeof(__ci_snapshot_process));

  p->recorded       = false;
  p->marked         = malloc(n_predecessors);
  p->message_queues = malloc(sizeof(__ci_queue) * n_predecessors);

  for ( int i = 0; i < n_predecessors; ++i )
    __ci_queue_init(&p->message_queues[i], sizeof(int));

  // all marked as false
  memset(p->marked, false, n_predecessors);

  if ( rank == initiator ) __ci_algo_take_snapshot(p, communicator);

  MPI_Status status;

  char marker;
  // safe to assume its 0
  int diff = 0;
  int count_diff;

  // while there are still incoming channels that have not been marked
  while ( !__ci_algo_snapshot_all_done(p->marked, n_predecessors) )
  {
    // receive a marker
    MPI_Recv(&marker, 1, MPI_CHAR, MPI_ANY_SOURCE, __CI_SNAPSHOT_TAG,
             *communicator, &status);

    __ci_algo_take_snapshot(p, communicator);

    // set the incoming channel as marked
    p->marked[__ci_adj_to_ind(status.MPI_SOURCE, predecessors,
                              n_predecessors)] = true;

    // this assumes that all incoming messages after the marker are considered
    // post snapshot
    // count the number of diff values, if its different than the previous
    // saved number of diff values it means that it has received an incoming
    // message
    count_diff = intdiff(p->state->incoming_messages_id,
                         __ci_state->incoming_messages_id, n_predecessors);
    if ( count_diff > diff )
    {
      // get correspondent index of the adjancecy
      //
      //
      //
      //
      // channel end????
      int adj_ind = __ci_adj_to_ind(__ci_state->channel_end, predecessors,
                                    n_predecessors);

      // p has been recorded and the incoming channel through which it has
      // received a basic message has not been marked
      // this makes some assumptions that might end up hurting me...
      // there is no guarantee of channel_end's atomicity
      if ( p->recorded && !p->marked[adj_ind] )
        __ci_queue_push(&p->message_queues[adj_ind], &__ci_state->state_bit);

      diff = count_diff;
    }
  }

  bool is_passive = p->state->state_bit & __CI_RECEIVING;

  // when the local snapshot was taken, the process was : {active, passive}
  __ci_print(ci_rank( ), "%s\n", is_passive ? "passive" : "active");

  MPI_Barrier(*communicator);

  for ( int i = 0; i < n_predecessors; ++i )
    __ci_queue_free(&p->message_queues[i]);

  free(p->message_queues);
  free(p->marked);

  return p;
}

void __ci_leave_tree(__ci_shavit_francez_node* node, MPI_Comm* communicator)
{
  char ack  = 'a';
  char wave = 'w';
  if ( !node->is_active && node->number_of_children == 0 )
  {
    if ( node->parent == __CI_UNDEF_PARENT )
    {
      MPI_Send(&ack, 0, MPI_CHAR, node->parent, __CI_ACK_TAG, *communicator);
      node->parent = __CI_UNDEF_PARENT;
    }
    // start wave tagged with p
    else
    {
      int n_successors = ci_get_number_successors( );
      int* successors  = ci_get_successors( );
      for ( int i = 0; i < n_successors; ++i )
        MPI_Send(&wave, 0, MPI_CHAR, successors[i], ci_rank( ), *communicator);
    }
  }
}

/* Termination Detection Algorithm
 *
 *
 * TODO!
 * */
void __ci_shavit_francez(int rank,
                         bool is_initiator,
                         __ci_shavit_francez_node* node,
                         __ci_snapshot_process* local_snapshot,
                         MPI_Comm* communicator)
{
  node->is_active          = is_initiator;
  node->parent             = is_initiator ? rank : __CI_UNDEF_PARENT;
  node->is_empty           = !is_initiator;
  node->number_of_children = 0;

  int diff_incoming = 0;
  int diff_outgoing = 0;
  int count_diff_incoming, count_diff_outgoing;
  int n_predecessors = ci_get_number_predecessors( );
  int n_successors   = ci_get_number_successors( );

  char ack  = 'a';
  char wave = 'w';
  int ack_probe_flag;
  int wave_probe_flag;
  MPI_Status status;
  while ( true )
  {
    count_diff_incoming =
        intdiff(local_snapshot->state->incoming_messages_id,
                __ci_state->incoming_messages_id, n_predecessors);
    count_diff_outgoing =
        intdiff(local_snapshot->state->outgoing_messages_id,
                __ci_state->outgoing_messages_id, n_successors);

    // p has sent a basic message
    if ( count_diff_outgoing > diff_outgoing )
    {
      __ci_print(ci_rank( ), "Sent a basic message\n");
      node->number_of_children++;
      diff_outgoing = count_diff_outgoing;
    }

    // p has received a basic message from neighbor q
    if ( count_diff_incoming > diff_incoming )
    {
      int neighbor_q = __ci_state->channel_end;
      __ci_print(ci_rank( ), "New incoming message");
      if ( !node->is_active )
      {
        node->is_active = true;
        node->parent    = neighbor_q;
      }
      else
      {
        __ci_print(ci_rank( ), "Sending ack to %d", neighbor_q);
        MPI_Send(&ack, 0, MPI_CHAR, neighbor_q, __CI_ACK_TAG, *communicator);
      }
      diff_incoming = count_diff_incoming;
    }

    MPI_Iprobe(MPI_ANY_SOURCE, __CI_ACK_TAG, *communicator, &ack_probe_flag,
               MPI_STATUS_IGNORE);
    // has an ack to receive
    if ( ack_probe_flag )
    {
      __ci_print(ci_rank( ), "Got ack\n");
      MPI_Recv(&ack, 0, MPI_CHAR, MPI_ANY_SOURCE, __CI_ACK_TAG, *communicator,
               MPI_STATUS_IGNORE);
      node->number_of_children--;
      __ci_leave_tree(node, communicator);
    }

    MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, *communicator, &wave_probe_flag,
               &status);

    if ( wave_probe_flag && status.MPI_TAG != __CI_ACK_TAG )
    {
      MPI_Recv(&wave, 0, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, *communicator,
               &status);

      if ( !node->is_active && node->number_of_children == 0 )
      {
      }
    }
  }
}

#define __CI_TOK_TAG 0
#define __CI_LDR_TAG 1
#define UNDEF        -1

echo_extinction* election_init( )
{
  echo_extinction* ss = malloc(sizeof(echo_extinction));

  ss->best_id         = UNDEF;
  ss->n_toks          = 0;
  ss->process_best_id = ci_rank( );
  ss->n_ldrs          = 0;
  ss->win_id          = UNDEF;

  // assign an UID to a process
  ss->id = ci_rank( );

  return ss;
}

int election(bool is_initiator, echo_extinction* ss, MPI_Comm* communicator)
{
  int p           = ci_rank( );
  int* neighbors  = ci_get_successors( );
  int n_neighbors = ci_get_number_successors( );

  bool exec_flag = true;

  int received_msg[2];
  MPI_Status status;

  if ( is_initiator )
  {
    ss->best_id = ss->id;
    int msg[2]  = {ss->id, p};
    for ( int i = 0; i < n_neighbors; ++i )
    {
      MPI_Send(&msg, 2, MPI_INT, neighbors[i], __CI_TOK_TAG, *communicator);
    }
  }

  while ( exec_flag )
  {
    MPI_Recv(&received_msg, 2, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG,
             *communicator, &status);

    int source = status.MPI_SOURCE;
    int tag    = status.MPI_TAG;

    // ldr message
    if ( tag == __CI_LDR_TAG )
    {
      if ( ss->n_ldrs == 0 )
      {
        for ( int i = 0; i < n_neighbors; ++i )
        {
          MPI_Send(&received_msg, 2, MPI_INT, neighbors[i], __CI_LDR_TAG,
                   *communicator);
        }
        ss->n_ldrs++;
        exec_flag  = false;
        ss->win_id = received_msg[0];
      }
    }
    // tok message
    else
    {
      // received ID is greater than active ID, re-init algorithm
      if ( received_msg[0] > ss->best_id )
      {
        ss->best_id         = received_msg[0];
        ss->n_toks          = 0;
        ss->process_best_id = received_msg[1];

        int to_send[2] = {ss->best_id, received_msg[1]};
        for ( int i = 0; i < n_neighbors; ++i )
        {
          if ( neighbors[i] != source )
            MPI_Send(&to_send, 2, MPI_INT, neighbors[i], __CI_TOK_TAG,
                     *communicator);
        }
      }
      // same ID, propagate
      else if ( received_msg[0] == ss->best_id )
      {
        ss->n_toks++;

        if ( ss->n_toks == n_neighbors - 1 )
        {
          if ( ss->best_id == ss->id )
          {
            int to_send[2] = {ss->id, p};
            for ( int i = 0; i < n_neighbors; ++i )
            {
              MPI_Send(&to_send, 2, MPI_INT, neighbors[i], __CI_LDR_TAG,
                       *communicator);
            }
          }
          else
          {
            int forward_parent[2] = {ss->best_id, ss->process_best_id};
            MPI_Send(&forward_parent, 2, MPI_INT, ss->process_best_id,
                     __CI_TOK_TAG, *communicator);
          }
        }
      }
    }
  }

  if ( false )
  {
    if ( ss->process_best_id == p )
      __ci_print(ci_rank( ), "Winner : [p:%d id:%d]\n", ss->process_best_id,
                 ss->win_id);
    else
      __ci_print(ci_rank( ), "Loser : [p:%d id:%d]\n", ss->process_best_id,
                 ss->win_id);
  }

  return ss->process_best_id;
}
