#include "cida.h"

// TODO
// char -> int8_t das constantes | valores int

// definir valor maximo para uma tag...
#include <assert.h>
#include <math.h>
#include <mpi.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "control.h"
#include "parser.h"
#include "queue.h"
#include "util.h"

/*-------------> Constants <---------------*/

#define __CI_ARG_VERBOSE   "--verbose"
#define __CI_ARG_TOPOLOGY  "--topology="
#define __CI_ARG_ALGORITHM "--algorithm="
#define __CI_ARG_ACTUATION "--actuation="

// Number of available datatypes
#define __CI_DATATYPES_COUNT ((int8_t) 15)

#define __CI_SYNC_MASTER ((char) 0)

// Nano sleep macro
#define __CI_NANO_SLEEP(S, NS) \
  nanosleep((const struct timespec[]){{(S), (NS)}}, NULL)
#define __CI_MS (1000000)

#define __CI_SWAP(x, y, T) \
  do                       \
  {                        \
    T tmp = (x);           \
    (x)   = (y);           \
    (y)   = tmp;           \
  } while ( 0 )

/* Types of control algorithms */
typedef enum
{
  ECHO,      // Done
  SNAPSHOT,  // Partially done
  DEADLOCK,
  TERMINATION,
  ELECTION,
  UNDEF_ALGO
} __ci_algorithms;

typedef struct __ci_control_arguments
{
  int probability_low;
  int probability_high;
  int s_to_sleep;
  int ns_to_sleep;
  __ci_vector* algorithms;
  bool verbose;
} __ci_control_arguments;

/*-------------> Static <---------------*/

// the value of a ci_datatype matches its index
// in mpi_datatypes
static MPI_Datatype mpi_datatypes[] = {
    MPI_CHAR,        MPI_SHORT,         MPI_INT,
    MPI_LONG,        MPI_LONG_LONG_INT, MPI_LONG_LONG,
    MPI_SIGNED_CHAR, MPI_UNSIGNED_CHAR, MPI_UNSIGNED_SHORT,
    MPI_UNSIGNED,    MPI_UNSIGNED_LONG, MPI_UNSIGNED_LONG_LONG,
    MPI_FLOAT,       MPI_DOUBLE,        MPI_LONG_DOUBLE,
    MPI_WCHAR};

static MPI_Comm global_communicator  = NULL;
static MPI_Comm control_communicator = NULL;

static __ci_graph* graph;
static pthread_t thread;

static __ci_control_arguments* control_arguments;

/*------------> Seeding <-----------*/
/*
 * Broadcasts the whole communicator
 * with a random seed.
 *
 *----------------------------------*/
void __ci_broadcast_random_seed( )
{
  int time_since_epoch;

  // get sync master's time since epoch
  if ( ci_rank( ) == __CI_SYNC_MASTER ) time_since_epoch = time(NULL);

  MPI_Bcast(&time_since_epoch, 1, MPI_INT, __CI_SYNC_MASTER,
            control_communicator);
  //  create a seed with the time from SYNC_MASTER
  __ci_srand(time_since_epoch);

  MPI_Barrier(control_communicator);
}

void __ci_prepare_comm_for_control(__ci_algorithms algorithm)
{
  // if provided, choose a random initiator
  // else choose a random process to play the role
  int random_initiator =
      graph->num_initiators > 0
          ? graph->initiators[__ci_rand( ) % graph->num_initiators]
          : __ci_rand( ) % ci_size( );

  __ci_snapshot_process* local_snapshot;
  switch ( algorithm )
  {
    case ECHO:
      __ci_print(ci_rank( ), "parent: %d\n",
                 __ci_algorithm_echo(random_initiator, __CI_WAVE_TAG,
                                     &control_communicator));
      break;
    case SNAPSHOT:
      local_snapshot =
          __ci_algorithm_snapshot(random_initiator, &control_communicator);
      free(local_snapshot);
      break;
    case DEADLOCK:
      if ( ci_rank( ) == 0 ) __ci_print(__CI_UTIL_IGNORE_RANK, "TODO\n");
      break;
    case TERMINATION:

      if ( ci_rank( ) == 0 ) __ci_print(__CI_UTIL_IGNORE_RANK, "TODO\n");

      if ( false )
      {
        int rank = ci_rank( );
        __ci_shavit_francez_node* node =
            malloc(sizeof(__ci_shavit_francez_node));

        local_snapshot =
            __ci_algorithm_snapshot(random_initiator, &control_communicator);

        __ci_shavit_francez(rank, graph->nodes[rank].is_initiator, node,
                            local_snapshot, &control_communicator);

        free(local_snapshot);
      }
      break;
    case ELECTION:
      echo_extinction* election_e = election_init( );

      election(graph->nodes[ci_rank( )].is_initiator, election_e,
               &control_communicator);

      free(election_e);

    default:
      break;
  }
}

/*-------------> Control <--------------*/
/*
 * Function that runs when a process creates
 * a new thread.
 *
 *--------------------------------------*/

void __ci_control( )
{
  __ci_broadcast_random_seed( );

  int rank = ci_rank( );

  const char* algorithms[] = {"Echo", "Snapshot", "Deadlock", "Termination",
                              "Election"};

  int iterator = 0;
  int algorithm;

// alternate between the first four
#define __CI_NUMBER_OF_ALTERNATING_ALGORITHMS 4

  while ( !(__ci_state->state_bit & __CI_FINISHED) )
  {
    algorithm = control_arguments->algorithms
                    ? control_arguments->algorithms->array[iterator]
                    : __ci_rand( ) % __CI_NUMBER_OF_ALTERNATING_ALGORITHMS;

    int random = __ci_rand( ) % control_arguments->probability_high;
    if ( random < control_arguments->probability_low )
    {
      MPI_Barrier(control_communicator);
      if ( rank == 0 )
        __ci_print(__CI_UTIL_IGNORE_RANK, "[  Start %s  ]\n",
                   algorithms[algorithm]);

      __ci_prepare_comm_for_control(algorithm);
      //__ci_prepare_comm_for_control(ELECTION);
      MPI_Barrier(control_communicator);

      if ( rank == 0 )
        __ci_print(__CI_UTIL_IGNORE_RANK, "[  End %s  ]\n",
                   algorithms[algorithm]);
      MPI_Barrier(control_communicator);
    }

    __CI_NANO_SLEEP(control_arguments->s_to_sleep,
                    control_arguments->ns_to_sleep);

    if ( control_arguments->algorithms )
      iterator = (iterator + 1) % control_arguments->algorithms->size;
  }
}

/*----------------> Send <--------------*/
/*
 * Function that replicates MPI_Send.
 * Starts with some fool proof checks.
 *
 * The send is wrapped around some update
 * states that ''blocks'' and ''unblocks''
 * the process while is doing the send.
 *
 * If the communication mode is UNDEF
 * it lets MPI decide which Send to use
 * (Ssend or Bsend)
 *
 *--------------------------------------*/
int ci_send(void* buffer,
            int count,
            ci_datatype datatype,
            int receiver,
            int tag,
            communication_mode mode)
{
  if ( !global_communicator )
    __ci_error("Sending Error: Communicator was not instantiated.\n");
  if ( count < 0 )
    __ci_error("Sending Error: Count should be a non negative integer.\n");
  if ( receiver < 0 || receiver >= ci_size( ) )
    __ci_error("Sending Error: Invalid Receiver.\n");
  if ( tag < 0 ) __ci_error("Sending Error: Invalid tag.\n");
  if ( datatype < 0 || datatype > __CI_DATATYPES_COUNT )
    __ci_error("Sending Error: Invalid datatype.\n");
  if ( tag == CI_ANY_TAG ) tag = MPI_ANY_TAG;

  int function_caller_rank = ci_rank( );

  // is there a direct edge between the receiver and the sender ?
  if ( !__ci_are_neighbors(graph, function_caller_rank, receiver) )
    __ci_error("Sending Error: Invalid communication.\n");

  // UPDATE STATE -> SENDING
  __ci_state->state_bit   = __CI_SENDING;
  __ci_state->channel_end = receiver;

  if ( mode == SYNC )
    MPI_Ssend(buffer, count, mpi_datatypes[datatype], receiver, tag,
              global_communicator);
  else if ( mode == UNDEF )
    MPI_Send(buffer, count, mpi_datatypes[datatype], receiver, tag,
             global_communicator);

  // new outgoing message in the channel [self, receiver]
  // analogous to a new incomming message in the channel [receiver, self]
  __ci_state->outgoing_messages_id[__ci_adj_to_ind(
      receiver, ci_get_successors( ), ci_get_number_successors( ))]++;

  // UPDATE STATE -> SENT
  __ci_state->state_bit = __CI_SENT;

  return EXIT_SUCCESS;
}

/*---------> Immediate Send <-----------*/
/*
 * Immediate return send.
 * The arrival of the message should be tested
 * with ci_wait() of the request
 *
 *--------------------------------------*/

int ci_isend(generic buffer,
             int count,
             ci_datatype datatype,
             int receiver,
             int tag,
             ci_request* request)
{
  if ( !global_communicator )
    __ci_error("ISending Error: Communicator was not instantiated.\n");
  if ( count < 0 )
    __ci_error("ISending Error: Count should be a non negative integer.\n");
  if ( receiver < 0 || receiver >= ci_size( ) )
    __ci_error("ISending Error: Invalid Receiver.\n");
  if ( tag < 0 ) __ci_error("Sending Error: Invalid tag.\n");
  if ( datatype < 0 || datatype > __CI_DATATYPES_COUNT )
    __ci_error("ISending Error: Invalid datatype.\n");
  if ( tag == CI_ANY_TAG ) tag = MPI_ANY_TAG;

  int function_caller_rank = ci_rank( );

  // is there a direct edge between the Receiver and the sender ?
  if ( !__ci_are_neighbors(graph, function_caller_rank, receiver) )
    __ci_error("ISending Error: Invalid communication.\n");

  // UPDATE STATE -> SENDING
  __ci_state->state_bit   = __CI_SENDING;
  __ci_state->channel_end = receiver;

  int ret_value;

  ret_value = MPI_Isend(buffer, count, mpi_datatypes[datatype], receiver, tag,
                        global_communicator, request);

  __ci_state->outgoing_messages_id[__ci_adj_to_ind(
      receiver, ci_get_successors( ), ci_get_number_successors( ))]++;

  // UPDATE STATE -> SENT
  __ci_state->state_bit = __CI_SENT;

  return ret_value ? CI_EXIT_FAILURE : CI_EXIT_SUCCESS;
}

/*--------------> Receive <--------------*/
/*
 * Function that replicates MPI_Recv.
 * Starts with some fool proof checks.
 *
 * The recv is wrapped around some update
 * states ''blocks'' and ''unblocks''
 * the process while is doing the recv.
 *
 *--------------------------------------*/
int ci_recv(generic buffer,
            int count,
            ci_datatype datatype,
            int sender,
            int tag,
            ci_status* status)
{
  if ( !global_communicator )
    __ci_error("Receiving Error: Communicator was not instantiated.\n");
  if ( count < 0 )
    __ci_error("Receiving Error: Count should be a non negative integer.\n");
  if ( datatype < 0 || datatype > __CI_DATATYPES_COUNT )
    __ci_error("Receiving Error: Invalid datatype.\n");
  if ( tag == CI_ANY_TAG )
    tag = MPI_ANY_TAG;
  else if ( tag < 0 )
    __ci_error("Receiving Error: Invalid tag.\n");

  int function_caller_rank = ci_rank( );

  // Valid sender
  if ( sender == CI_ANY_SOURCE )
    sender = MPI_ANY_SOURCE;
  else if ( sender < 0 || sender >= ci_size( ) )
    __ci_error("Receiving Error: Invalid rank\n");
  else if ( !__ci_are_neighbors(graph, sender, function_caller_rank) )
    __ci_error("Receiving Error: Invalid communication.\n");

  MPI_Status stat;

  // UPDATE STATE -> RECEIVING
  __ci_state->state_bit   = __CI_RECEIVING;
  __ci_state->channel_end = sender;

  MPI_Recv(buffer, count, mpi_datatypes[datatype], sender, tag,
           global_communicator, &stat);

  // UPDATE STATE -> RECEIVED
  __ci_state->state_bit   = __CI_RECEIVED;
  __ci_state->channel_end = stat.MPI_SOURCE;

  // new incomming message in the channel [self, sender]
  __ci_state->incoming_messages_id[__ci_adj_to_ind(
      stat.MPI_SOURCE, ci_get_predecessors( ),
      ci_get_number_predecessors( ))]++;

  // if there was a status to begin with ie. ci_recv(..., NULL);
  if ( status )
  {
    status->ci_tag    = stat.MPI_TAG;
    status->ci_source = stat.MPI_SOURCE;
  }

  return EXIT_SUCCESS;
}

/*---------> Immediate Receive <-----------*/
/*
 * Immediate return receive.
 * The receipt of the message should be tested
 * with ci_wait() of the request
 *
 *-----------------------------------------*/

int ci_irecv(generic buffer,
             int count,
             ci_datatype datatype,
             int sender,
             int tag,
             ci_request* request)
{
  if ( !global_communicator )
    __ci_error("Receiving Error: Communicator was not instantiated.\n");
  if ( count < 0 )
    __ci_error("Receiving Error: Count should be a non negative integer.\n");
  if ( datatype < 0 || datatype > __CI_DATATYPES_COUNT )
    __ci_error("Receiving Error: Invalid datatype.\n");
  if ( tag == CI_ANY_TAG )
    tag = MPI_ANY_TAG;
  else if ( tag < 0 )
    __ci_error("Receiving Error: Invalid tag.\n");

  int function_caller_rank = ci_rank( );

  // Valid sender ?
  if ( sender == CI_ANY_SOURCE )
    sender = MPI_ANY_SOURCE;
  else if ( sender < 0 || sender >= ci_size( ) )
    __ci_error("Receiving Error: Invalid rank\n");
  else if ( !__ci_are_neighbors(graph, sender, function_caller_rank) )
    __ci_error("Receiving Error: Invalid communication.\n");

  // UPDATE STATE -> RECEIVING
  __ci_state->state_bit   = __CI_RECEIVING;
  __ci_state->channel_end = sender;

  MPI_Irecv(buffer, count, mpi_datatypes[datatype], sender, tag,
            global_communicator, request);

  return EXIT_SUCCESS;
}

int ci_wait(ci_request* request, ci_status* status)
{
  MPI_Status _status;
  MPI_Wait(request, !status ? MPI_STATUS_IGNORE : &_status);

  // new incoming message on the channel [self, source]
  __ci_state->incoming_messages_id[__ci_adj_to_ind(
      _status.MPI_SOURCE, ci_get_predecessors( ),
      ci_get_number_predecessors( ))]++;

  // UPDATE STATE -> RECEIVED
  __ci_state->state_bit = __CI_RECEIVED;

  if ( status )
  {
    status->ci_source = _status.MPI_SOURCE;
    status->ci_tag    = _status.MPI_TAG;
  }

  return EXIT_SUCCESS;
}

int ci_waitall(int count, ci_request requests[], ci_status status[])
{
  MPI_Status _status[count];
  MPI_Waitall(count, requests, !status ? MPI_STATUS_IGNORE : _status);

  if ( status )
  {
    for ( int i = 0; i < count; ++i )
    {
      status[i].ci_source = _status[i].MPI_SOURCE;
      status[i].ci_tag    = _status[i].MPI_TAG;
    }
  }

  return EXIT_SUCCESS;
}

/*---------------> Init <---------------*/
/*
 * Initializes the MPI.
 * Parses the arguments.
 * Reads file with topology.
 * Parses the file.
 * Creates a thread.
 * Some other initializations.
 *
 *--------------------------------------*/

// problems arise when trying all processes try to open
// the same file at the "same" time.
// find a better way to read the topology file.
// maybe using MPI_File(..)
int ci_init(int* argc, char*** argv)
{
  int provided;
  // this causes alot of memory leaks
  MPI_Init_thread(argc, argv, MPI_THREAD_MULTIPLE, &provided);

  char* filename    = NULL;
  control_arguments = malloc(sizeof(__ci_control_arguments));

  control_arguments->probability_low  = 50;
  control_arguments->probability_high = 100;
  control_arguments->ns_to_sleep      = 0;
  control_arguments->s_to_sleep       = 0;
  control_arguments->verbose          = false;
  control_arguments->algorithms       = NULL;

  bool valid_argument;
  int n_args = *(argc);

  // think about a new argument
  // -w meaning a weighted graph
  //  - allows mst
  // -p meaning probabilistic signaled control
  // -c meaning a clock signaled control
  // by default, it will alternate between those two
  for ( int i = 1; i < n_args; ++i )
  {
#define __CI_NTH_ARGUMENT             (*(*(argv) + i))
#define __CI_NCMP_NTH_ARGV(argv, cmp) (!strncmp(argv, cmp, sizeof(cmp) - 1))

    valid_argument = false;
    if ( __CI_NCMP_NTH_ARGV(__CI_NTH_ARGUMENT, __CI_ARG_TOPOLOGY) )
    {
      int topology_prefix_len = sizeof(__CI_ARG_TOPOLOGY);
      // alloc enough size for filename and 0x0
      filename = calloc(sizeof(char),
                        (strlen(__CI_NTH_ARGUMENT) - topology_prefix_len + 1));

      // extract topology file name
      strncpy(filename, __CI_NTH_ARGUMENT + topology_prefix_len - 1,
              (strlen(__CI_NTH_ARGUMENT)) - topology_prefix_len + 1);

      valid_argument = true;
      n_args--;
      i--;
    }
    else if ( __CI_NCMP_NTH_ARGV(__CI_NTH_ARGUMENT, __CI_ARG_VERBOSE) )
    {
      control_arguments->verbose = true;

      valid_argument = true;
      n_args--;
      i--;
    }
    else if ( __CI_NCMP_NTH_ARGV(__CI_NTH_ARGUMENT, __CI_ARG_ACTUATION) )
    {
      // size of the prefix
      int actuation_prefix_len = sizeof(__CI_ARG_ACTUATION);
      // size of the sufix
      int argument_len = (strlen(__CI_NTH_ARGUMENT) - actuation_prefix_len + 1);
      char* cropped_arg = calloc(sizeof(char), argument_len);

      // copy the sufix of the argument
      strncpy(cropped_arg, __CI_NTH_ARGUMENT + actuation_prefix_len - 1,
              argument_len);

      char* type_of_actuator = strsep(&cropped_arg, ",");
      char* sub_arg_one      = strsep(&cropped_arg, ",");
      char* sub_arg_two      = strsep(&cropped_arg, ",");

      if ( type_of_actuator == NULL || sub_arg_one == NULL ||
           sub_arg_two == NULL )
        __ci_error("Error: Invalid format of actuator.\n");

      if ( !strncmp(type_of_actuator, "probability", sizeof("probability")) )
      {
        char* end_ptr_l;
        char* end_ptr_h;
        int low  = strtol(sub_arg_one, &end_ptr_l, 10);
        int high = strtol(sub_arg_two, &end_ptr_h, 10);

        if ( end_ptr_l == sub_arg_one )
          __ci_error("Error: Invalid probability bottom range.\n");

        if ( end_ptr_h == sub_arg_two )
          __ci_error("Error: Invalid probability upper range.\n");

        if ( low > high || low < 0 || high <= 0 )
          __ci_error("Error: Invalid probability.\n");

        control_arguments->probability_low  = low;
        control_arguments->probability_high = high;
      }

      // argument of type clock,<time value>,<time unit>
      if ( !strncmp(type_of_actuator, "clock", sizeof("clock")) )
      {
        char* end_ptr;
        int time = strtol(sub_arg_one, &end_ptr, 10);

        if ( end_ptr == sub_arg_one )
          __ci_error("Error: Invalid time value.\n");

        if ( !strncmp(sub_arg_two, "s", sizeof("s")) )
          control_arguments->s_to_sleep = time;
        else if ( !strncmp(sub_arg_two, "ns", sizeof("ns")) )
          control_arguments->ns_to_sleep = time;
        else
          __ci_error("Error: Invalid time unit\n");
      }

      valid_argument = true;
      n_args--;
      i--;
    }
    else if ( __CI_NCMP_NTH_ARGV(__CI_NTH_ARGUMENT, __CI_ARG_ALGORITHM) )
    {
      int algorithm_prefix_len = sizeof(__CI_ARG_ALGORITHM);
      char* cropped_arg        = calloc(
                 sizeof(char), (strlen(__CI_NTH_ARGUMENT) - algorithm_prefix_len + 1));

      strncpy(cropped_arg, __CI_NTH_ARGUMENT + algorithm_prefix_len - 1,
              (strlen(__CI_NTH_ARGUMENT)) - algorithm_prefix_len + 1);

      __ci_vector* algorithms = __ci_vector_init( );

      char* token = strsep(&cropped_arg, ",");

      while ( token != NULL )
      {
        if ( !strncmp(token, "echo", sizeof("echo")) )
          __ci_vector_push_back(algorithms, ECHO);
        else if ( !strncmp(token, "snapshot", sizeof("snapshot")) )
          __ci_vector_push_back(algorithms, SNAPSHOT);
        else if ( !strncmp(token, "deadlock", sizeof("deadlock")) )
          __ci_vector_push_back(algorithms, DEADLOCK);
        else if ( !strncmp(token, "termination", sizeof("termination")) )
          __ci_vector_push_back(algorithms, TERMINATION);

        token = strsep(&cropped_arg, ",");
      }

      control_arguments->algorithms = algorithms;

      valid_argument = true;
      n_args--;
      i--;
    }
#undef __CI_NCMP_NTH_ARGV
#undef __CI_NTH_ARGUMENT

    // remove the argument
    if ( valid_argument )
    {
      // shift the argument to the end
      for ( int j = i + 1; j < *argc; ++j )
        __CI_SWAP(*(*(argv) + j), *(*(argv) + j + i), char*);

      // clear the bytes
      memset(*(*argv) + n_args, 0, strlen(*(*argv) + n_args));
    }
  }
  *(argc) = n_args;

  /* read file */
  __ci_file* read_topology =
      __ci_read_topology_file(filename ? filename : DEFAULT_FILENAME);

  global_communicator = MPI_COMM_WORLD;

  /* duplicate global_comm to control_comm */
  /* processors in the control communicate using this new comm */
  MPI_Comm_dup(global_communicator, &control_communicator);

  int size;
  MPI_Comm_size(global_communicator, &size);

  /* get the topology from file */
  graph = __ci_parse_file(read_topology, size);

  if ( !graph )
  {
    if ( read_topology ) free(read_topology);
    if ( filename ) free((void*) filename);
    __ci_error("Could not create graph\n");
  }
  /* if it was allocated, free */
  if ( filename ) free((void*) filename);

  /* won't be used anymore */
  if ( read_topology ) free(read_topology);

  /* init state */
  __ci_state            = malloc(sizeof(__ci_process_state));
  __ci_state->state_bit = __CI_IDLE;

  // alloc and set at 0
  __ci_state->incoming_messages_id =
      calloc(sizeof(int), ci_get_number_predecessors( ));
  __ci_state->outgoing_messages_id =
      calloc(sizeof(int), ci_get_number_successors( ));

  /* thread the process and send it to control loop */
  if ( pthread_create(&thread, NULL, (void*) __ci_control, NULL) )
    __ci_error("Process could not create a thread\n");

  return EXIT_SUCCESS;
}

/*---------------> Rank <---------------*/
/*
 * Returns the rank of the current process
 *
 *--------------------------------------*/
int ci_rank( )
{
  int rank;
  MPI_Comm_rank(global_communicator, &rank);
  return rank;
}
/*---------------> Size <---------------*/
/*
 * Returns the size of the global communicator
 *
 *--------------------------------------*/
int ci_size( )
{
  int size;
  MPI_Comm_size(global_communicator, &size);

  return size;
}

/*--------------> Barrier <-------------*/
/*
 * Barrier for other processes.
 *
 *--------------------------------------*/
int ci_barrier( )
{
  __ci_state->state_bit = __CI_BARRIER;
  MPI_Barrier(global_communicator);
  return EXIT_SUCCESS;
}

int ci_probe(int sender, int tag, ci_status* status)
{
  MPI_Status mpi_status;

  if ( !global_communicator )
    __ci_error("Probing Error: Communicator was not instantiated.\n");
  if ( tag == CI_ANY_TAG )
    tag = MPI_ANY_TAG;
  else if ( tag < 0 )
    __ci_error("Probing Error: Invalid tag.\n");

  int function_caller_rank = ci_rank( );

  if ( sender == CI_ANY_SOURCE )
    sender = MPI_ANY_SOURCE;
  else if ( sender < 0 || sender >= ci_size( ) )
    __ci_error("Probing Error: Invalid rank\n");
  else if ( !__ci_are_neighbors(graph, sender, function_caller_rank) )
    __ci_error("Probing Error: Invalid communication.\n");

  if ( MPI_Probe(sender, tag, global_communicator, &mpi_status) != MPI_SUCCESS )
    return EXIT_FAILURE;

  if ( status )
  {
    status->ci_source = mpi_status.MPI_SOURCE;
    status->ci_tag    = mpi_status.MPI_TAG;
  }

  return EXIT_SUCCESS;
}

int ci_iprobe(int sender, int tag, int* flag, ci_status* status)
{
  MPI_Status mpi_status;

  if ( !global_communicator )
    __ci_error("Probing Error: Communicator was not instantiated.\n");
  if ( tag == CI_ANY_TAG )
    tag = MPI_ANY_TAG;
  else if ( tag < 0 )
    __ci_error("Probing Error: Invalid tag.\n");

  int function_caller_rank = ci_rank( );

  if ( sender == CI_ANY_SOURCE )
    sender = MPI_ANY_SOURCE;
  else if ( sender < 0 || sender >= ci_size( ) )
    __ci_error("Probing Error: Invalid rank\n");
  else if ( !__ci_are_neighbors(graph, sender, function_caller_rank) )
    __ci_error("Probing Error: Invalid communication.\n");

  if ( MPI_Iprobe(sender, tag, global_communicator, flag, &mpi_status) !=
       MPI_SUCCESS )
    return EXIT_FAILURE;

  if ( status )
  {
    status->ci_source = mpi_status.MPI_SOURCE;
    status->ci_tag    = mpi_status.MPI_TAG;
  }

  return EXIT_SUCCESS;
}

// candidatos a leader, arg
int ci_election(int unique_id)
{
  int leader;
  echo_extinction* election_e = election_init( );
  election_e->id              = unique_id;

  leader = election(graph->nodes[ci_rank( )].is_initiator, election_e,
                    &global_communicator);

  free(election_e);

  return leader;
}

/*-----------> Get Topology <-------------*/
/*
 * Functions that return some aspects of the
 * graph's topology.
 *
 *----------------------------------------*/
int* ci_get_successors( ) { return graph->nodes[ci_rank( )].successors->array; }

int* ci_get_predecessors( )
{
  return graph->nodes[ci_rank( )].predecessors->array;
}

int ci_get_number_successors( )
{
  return graph->nodes[ci_rank( )].successors->size;
}

int ci_get_number_predecessors( )
{
  return graph->nodes[ci_rank( )].predecessors->size;
}

void ci_print_graph( ) { __ci_print_neighbors(graph, ci_size( )); }

/*-------------> Finalize <-------------*/
/*
 * Kills thread.
 * Clean up.
 *
 *--------------------------------------*/

int ci_finalize( )
{
  __ci_state->state_bit = __CI_FINISHED;

  pthread_cancel(thread);

  if ( thread ) pthread_join(thread, NULL);

  int comm_size;
  if ( global_communicator ) MPI_Comm_size(global_communicator, &comm_size);

  if ( graph ) __ci_free_nodes(graph, comm_size);

  MPI_Comm_free(&control_communicator);

  free(__ci_state->incoming_messages_id);
  free(__ci_state->outgoing_messages_id);
  free(__ci_state);

  if ( control_arguments->algorithms )
    __ci_vector_free(control_arguments->algorithms);
  free(control_arguments);

  __ci_print(ci_rank( ), "Finalizing\n");

  return MPI_Finalize( );
}

