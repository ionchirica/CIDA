#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "cida.h"
#include "queue.h"

/* -------------> Philosophers 2 <---------------- *
 *                                                 *
 * A better approach to solve the philosophers     *
 * dilema.                                         *
 * Consider the termination of a sending event     *
 * as the completion of the communication          *
 *                                                 *
 * If a philosopher tries to pick up a fork and    *
 * the latter accepts the request, the philosopher *
 * owns the fork.                                  *
 * For some reason, it does not deadlock :).       *
 *                                                 *
 * ----------------------------------------------- */

// NECESSIDADE DE SABER A TOPOLOGIA DA REDE. ??

/*----------> Constants <----------*/
#define DEBUG           1
#define MASTER          0
#define SLEEP_THRESHOLD 6 /* [0->6] seconds to sleep */

#define FREE 0
#define BUSY 1

#define THINKING 0
#define EATING   1

#define REQUEST  0
#define RESPONSE 1
#define RELEASE  2

/*---------> Skeletons <---------*/
void do_fork(int rank, int number_of_iterations);
void do_philosopher(int rank, int number_of_iterations);

int main(int argc, char** argv)
{
  int size;
  ci_init(&argc, &argv);
  int number_of_iterations;

  if ( argc == 2 )
  {
    int arg_iterations   = atoi(argv[1]);
    number_of_iterations = arg_iterations > 0 ? arg_iterations : -1;
  }
  else
  {
    fprintf(stderr, "Usage: %s <number of iterations>\n", argv[0]);
    return EXIT_FAILURE;
  }

  size = ci_size( );

  int rank = ci_rank( );
  ci_barrier( );

  if ( (rank % 2) == 1 )
    do_fork(rank, number_of_iterations * 2);
  else
    do_philosopher(rank, number_of_iterations);

  ci_barrier( );
  ci_finalize( );

  return EXIT_SUCCESS;
}

/*---------> Fork <---------*/
/* Receive incoming messages,
 * If the fork is available, block it and reply
 * If the fork is busy and being released, unblock it and reply
 *--------------------------*/

// um filosofo nao pousa um garfo que nao tem
void do_fork(int rank, int number_of_iterations)
{
  ci_status status;
  char request;
  int availability = FREE;

  int source = CI_ANY_SOURCE;

  int release_flag;
  int request_flag;
  while ( number_of_iterations == -1 ? true : number_of_iterations )
  {
    ci_iprobe(source, RELEASE, &release_flag, &status);
    ci_iprobe(source, REQUEST, &request_flag, &status);

    if ( request_flag )
    {
      if ( availability == FREE )
      {
        ci_recv(&request, 1, ci_char, source, REQUEST, &status);
        printf("[Fork#%d] is busy. Requested by [Phil#%d]\n", rank,
               status.ci_source);
        availability = BUSY;
      }
    }
    if ( release_flag )
    {
      if ( availability == BUSY )
      {
        ci_recv(&request, 1, ci_char, CI_ANY_SOURCE, RELEASE, &status);

        printf("[Fork#%d] is free. Released by [Phil#%d]\n", rank,
               status.ci_source);
        availability = FREE;
        number_of_iterations--;
      }
    }
  }
}

/* ---------> Philosopher <--------- */
/* Think
 * Ask for left fork
 * Ask for right fork
 * Eat
 * Release left fork
 * Release right fork
 * --------------------------------- */
void do_philosopher(int rank, int number_of_iterations)
{
  char request_fork = 'r';
  char release_fork = 'f';

  int left_fork  = rank == 0 ? ci_size( ) - 1 : rank - 1;
  int right_fork = rank == ci_size( ) - 1 ? 0 : rank + 1;

  srand(time(NULL) + rank * rank);

  while ( number_of_iterations == -1 ? true : number_of_iterations-- )
  {
    printf("[Phil#%d] is thinking\n", rank);

    sleep(rand( ) % SLEEP_THRESHOLD);

#if DEBUG
    printf("[Phil#%d] trying to pick up [Fork#%d]\n", rank, right_fork);
#endif

    ci_send(&request_fork, 1, ci_char, right_fork, REQUEST, SYNC);

#if DEBUG
    printf("[Phil#%d] has [Fork#%d]\n", rank, right_fork);

    printf("[Phil#%d] trying to pick up [Fork#%d]\n", rank, left_fork);
#endif
    ci_send(&request_fork, 1, ci_char, left_fork, REQUEST, SYNC);

#if DEBUG
    printf("[Phil#%d] has [Fork#%d]\n", rank, left_fork);
#endif

    sleep(rand( ) % SLEEP_THRESHOLD);

    printf("[Phil#%d] is eating\n", rank);

    ci_send(&release_fork, 1, ci_char, right_fork, RELEASE, SYNC);
#if DEBUG
    printf("[Phil#%d] released [Fork#%d]\n", rank, right_fork);
#endif

    ci_send(&release_fork, 1, ci_char, left_fork, RELEASE, SYNC);
#if DEBUG
    printf("[Phil#%d] released [Fork#%d]\n", rank, left_fork);
#endif
  }
}

