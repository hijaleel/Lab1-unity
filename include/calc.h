#include <stdbool.h>
#include <stddef.h>

#ifndef CALC_H
#define CALC_H

#ifdef __cplusplus
extern "C" {    
#endif

int add(int x, int y);
int sub(int x, int y);
int mul(int x, int y);
int divide(int x, int y);
int mod(int x, int y);
int power_two(int x);
int power(int x, int n);
bool is_equal(int x, int y);
bool is_greater(int x, int y);
bool is_smaller(int x, int y);

/*
 * [QUESTION 2 REQUIREMENT - 4 MARKS]
 * magnitude: Computes the absolute magnitude of a signed integer
 * negnum: Helper function returning 1 if negative, 0 otherwise
 */
int magnitude(int x);
int negnum(int x);

void fill_single_digit_positive_number(int arr[], size_t size);
const char *true_as_string();
const char *false_as_string();

#ifdef __cplusplus
}   
#endif

#endif
