#include "util.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

void __ci_error(const char* error_message)
{
  fprintf(stderr, error_message);
  exit(EXIT_FAILURE);
}

// reverse a string, no real use for now...
char* reverse_string(const char* to_reverse, int size)
{
  char* reversed = malloc(sizeof(char) * size);
  memcpy(reversed, to_reverse, size);

  for ( int i = 0; i < (size / 2); ++i )
  {
    reversed[i] ^= reversed[size - i - 1];
    reversed[size - i - 1] ^= reversed[i];
    reversed[i] ^= reversed[size - i - 1];
  }

  return reversed;
}

// count the number of different elements
// in an equal sized array of integers
int intdiff(int* s1, int* s2, int len)
{
  int* p = s1;
  int* q = s2;

  int ret = 0;

  if ( s1 == s2 ) return ret;

  while ( len > 0 )
  {
    if ( *p != *q ) ret++;
    len--;
    p++;
    q++;
  }
  return ret;
}

static unsigned long __ci_next_rand = 1;
int __ci_rand( )
{
  __ci_next_rand = __ci_next_rand * 1103515245 + 12345;
  return ((unsigned) (__ci_next_rand / 65536) % 32768);
}

void __ci_srand(unsigned long seed) { __ci_next_rand = seed; }

void __ci_print(int rank, char* format, ...)
{
  va_list va;

  char time[30];
  int millisec;

  struct timeval tv;
  gettimeofday(&tv, NULL);

  millisec = lrint(tv.tv_usec / 1000.0);
  if ( millisec >= 1000 )
  {
    millisec -= 1000;
    tv.tv_sec++;
  }

  strftime(time, 30, "[%H:%M:%S", localtime(&tv.tv_sec));
  if ( rank == __CI_UTIL_IGNORE_RANK )
    fprintf(stdout, "%s.%03d] ", time, millisec);
  else
    fprintf(stdout, "%s.%03d - P#%d] ", time, millisec, rank);

  va_start(va, format);
  vfprintf(stdout, format, va);
  va_end(va);
}

// TODO
void __ci_print_verbose(int rank, char* format, ...)
{
  va_list va;

  va_start(va, format);

  vfprintf(stdout, format, va);

  va_end(va);
}
