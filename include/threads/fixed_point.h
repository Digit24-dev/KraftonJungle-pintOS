#pragma once
#include <stdint.h>

/* advanced */

/* Convert n to fixed point */
int itofp(int n);

/* Convert x to integer (rounding toward zero) */
int fptoi(int x);

/* Convert x to integer (rounding to nearest) */
int fptoi_r(int x);

/* Add (x + y) */
int fp_add(int x, int y);

/* Subtract (x - y) */
int fp_sub(int x, int y);

/* Add (x + n) */
int fp_add2(int x, int n);

/* Subtract (x - n) */
int fp_sub2(int x, int n);

/* Multi (x * y) -> ((int64_t)x)*y */
int fp_multi(int x, int y);

/* Multi (x * n) */
int fp_multi2(int x, int n);

/* Divide (x / y) -> ((int64_t)x) * f/y */
int fp_div(int x, int y);

/* Divide (x / n) */
int fp_div2(int x, int n);
/* advanced */