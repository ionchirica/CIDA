#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "cida.h"

/* ---------------> Philosophers <------------- *
 *                                              *
 * A not so smart way to solve the philosophers *
 * dilema.                                      *
 * For some reason, it deadlocks :).            *
 *                                              *
 * -------------------------------------------- */

/*----------> Constants <----------*/
#define DEBUG           1
#define MASTER          0
#define SLEEP_THRESHOLD 10 /* [0->14] seconds to sleep */

#define FREE 0
#define BUSY 1

#define THINKING 0
#define EATING   1

#define REQUEST  0
#define RESPONSE 1
#define RELEASE  2

/*---------> Skeletons <---------*/
void do_fork(int rank, int forks[], int neighbors[]);
void do_philosopher(int rank, int philosophers[], int neighbors[]);

int main(int argc, char** argv)
{
  int size;
  ci_init(&argc, &argv);

  size = ci_size( );

  int rank = ci_rank( );

  int philosophers[size];
  int forks[size];

  for ( int i = 0; i < size; ++i )
  {
    philosophers[i] = THINKING;
    forks[i]        = FREE;
  }

  int* neighbors = ci_get_successors( );

  if ( rank != MASTER )
  {
    if ( (rank % 2) == 1 )
    {
      // printf("I am a fork %d\n", rank);
      do_fork(rank, forks, neighbors);
    }
    else
    {
      // printf("I am a philosopher %d\n", rank);
      do_philosopher(rank, philosophers, neighbors);
    }
  }

  ci_barrier( );
  ci_finalize( );

  return EXIT_SUCCESS;
}

/*---------> Fork <---------*/
/* Recieve incoming messages,
 * If the fork is available, block it and reply
 * If the fork is busy and being released, unblock it and reply
 *--------------------------*/

void do_fork(int rank, int forks[], int neighbors[])
{
  ci_status status;
  int request_source;
  char request;
  char response;
  __ci_vector* msg_queue = __ci_vector_init( );

  while ( true )
  {
    ci_recv(&request, 1, ci_char, CI_ANY_SOURCE, CI_ANY_TAG, &status);

    if ( status.ci_tag == REQUEST && forks[rank] == FREE )
    {
#if DEBUG
      printf("Fork: %d is busy. Signaled by %d \n", rank, status.ci_source);
#endif
      forks[rank] = BUSY;

      // Reply: I am yours.
      ci_send(&response, 1, ci_char, status.ci_source, RESPONSE, UNDEF);
    }
    else if ( status.ci_tag == RELEASE && forks[rank] == BUSY )
    {
#if DEBUG
      printf("Fork: %d is free.\n", rank);
#endif
      forks[rank] = FREE;

      // Reply: I am no longer yours.
      ci_send(&response, 1, ci_char, status.ci_source, RESPONSE, UNDEF);
    }
  }
}

/* ---------> Philosopher <--------- */
/* Ask for left fork
 * Ask for right fork
 * Wait for their response
 * Eat
 * Release left fork
 * Release right fork
 * Think
 * --------------------------------- */
void do_philosopher(int rank, int philosophers[], int neighbors[])
{
#if DEBUG
  printf("My state: %d\n", philosophers[rank]);
#endif

  char response     = 0;
  char request_fork = rank;
  char release_fork = rank;

  srand(time(NULL) + rank);

  ci_status status;
  while ( true )
  {
    printf("I am thinking %d\n", rank);

    sleep(rand( ) % SLEEP_THRESHOLD);

#if DEBUG
    printf("%d Requested %d\n", rank, neighbors[0]);
#endif
    ci_send(&request_fork, 1, ci_char, neighbors[0], REQUEST, UNDEF);
    ci_recv(&response, 1, ci_char, neighbors[0], RESPONSE, &status);

#if DEBUG
    printf("%d Requested %d\n", rank, neighbors[1]);
#endif
    ci_send(&request_fork, 1, ci_char, neighbors[1], REQUEST, UNDEF);
    ci_recv(&response, 1, ci_char, neighbors[1], RESPONSE, &status);

    sleep(rand( ) % SLEEP_THRESHOLD);

    printf("I am eating %d\n", rank);
    philosophers[rank] = EATING;

#if DEBUG

    printf("%d Released %d\n", rank, neighbors[0]);
    ci_send(&release_fork, 1, ci_char, neighbors[0], RELEASE, UNDEF);

    printf("%d Released %d\n", rank, neighbors[1]);
    ci_send(&release_fork, 1, ci_char, neighbors[1], RELEASE, UNDEF);
#endif

    philosophers[rank] = THINKING;
  }
}

