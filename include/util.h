#ifndef __UTIL_H
#define __UTIL_H

#define __CI_UTIL_IGNORE_RANK ((int8_t) -1)

void __ci_error(const char* error_message);

/*
 * Reverse a string
 * Useful keeping in mind the byte order
 * Used to compare a string with a multichar valuek
 * */
char* reverse_string(const char* to_reverse, int size);

//
int intdiff(int* s1, int* s2, int len);

int __ci_rand( );
void __ci_srand(unsigned long seed);

void __ci_print(int rank, char* format, ...);

void __ci_print_verbose(int rank, char* format, ...);

#endif
