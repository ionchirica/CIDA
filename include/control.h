#ifndef __CONTROL_H
#define __CONTROL_H

#include <mpi.h>
#include <stdbool.h>

#include "parser.h"
#include "queue.h"
//
// Initial state is idle
#define __CI_IDLE ((int8_t) 0)

// Control communication constants
#define __CI_SENDING   (1L << 0)
#define __CI_SENT      (1L << 1)
#define __CI_RECEIVING (1L << 2)
#define __CI_RECEIVED  (1L << 3)
#define __CI_BARRIER   (1L << 4)
#define __CI_FINISHED  (1L << 5)

#define __CI_WAVE_TAG     ((int8_t) 4)
#define __CI_SNAPSHOT_TAG ((int8_t) 5)
#define __CI_ACK_TAG      ((int8_t) 6)

// local id
#define __CI_ELECTION_TAG_ID ((int8_t) 1)
// election id
#define __CI_ELECTION_TAG_EL ((int8_t) 2)
// parity
#define __CI_ELECTION_TAG_PA ((int8_t) 3)
// booleans
#define __CI_ELECTION_TAG_BO ((int8_t) 4)

#define __CI_UNDEF_PARENT ((int8_t) -1)

/* The state of a process */
typedef struct __ci_process_state
{
  int state_bit;

  // P2P communication
  // receiver or sender
  int channel_end;

  /* ID's of messages */
  int* incoming_messages_id;
  int* outgoing_messages_id;
} __ci_process_state;

/* Snapshot helper */
typedef struct __ci_snapshot_process
{
  bool recorded;
  bool* marked;
  __ci_process_state* state;

  // queue of altered states
  // after receiving a marker
  __ci_queue* message_queues;
} __ci_snapshot_process;

typedef struct __ci_shavit_francez_node
{
  bool is_active;
  int number_of_children;
  int parent;
  bool is_empty;
} __ci_shavit_francez_node;

typedef struct echo_extinction
{
  int best_id;
  int n_toks;
  int process_best_id;
  int n_ldrs;
  int win_id;
  int id;
} echo_extinction;

// extern to be used in control.c and cida.c
extern __ci_process_state* __ci_state;

int __ci_adj_to_ind(int process_to_find,
                    int* target_array,
                    int size_taget_array);

int __ci_algorithm_echo(int initiator, int tag, MPI_Comm* communicator);

__ci_snapshot_process* __ci_algorithm_snapshot(int initiator,
                                               MPI_Comm* communicator);

void __ci_algo_take_snapshot(__ci_snapshot_process* process,
                             MPI_Comm* communicator);

bool __ci_algo_snapshot_all_done(bool* markers, int size_markers);

void __ci_shavit_francez(int rank,
                         bool is_initiator,
                         __ci_shavit_francez_node* node,
                         __ci_snapshot_process* local_snapshot,
                         MPI_Comm* communicator);

echo_extinction* election_init( );

int election(bool is_initiator, echo_extinction* ss, MPI_Comm* communicator);
#endif
