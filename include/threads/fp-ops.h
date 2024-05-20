#pragma once
#include <stdint.h>

typedef int fp_float;
typedef int64_t fp_double;

/* Convert n to fixed point */
fp_float itofp(int n);

/* Convert x to integer (rounding toward zero) */
int fptoi(fp_float x);

/* Convert x to integer (rounding to nearest) */
int fptoi_r(fp_float x);

/* Add (x + y) */
fp_float fp_add(fp_float x, fp_float y);

/* Subtract (x - y) */
fp_float fp_sub(fp_float x, fp_float y);

/* Add (x + n) */
fp_float fp_add2(fp_float x, int n);

/* Subtract (x - n) */
fp_float fp_sub2(fp_float x, int n);

/* Multi (x * y) -> ((int64_t)x)*y */
fp_float fp_multi(fp_float x, fp_float y);

/* Multi (x * n) */
fp_float fp_multi2(fp_float x, int n);

/* Divide (x / y) -> ((int64_t)x) * f/y */
fp_float fp_div(fp_float x, fp_float y);

/* Divide (x / n) */
fp_float fp_div2(fp_float x, int n);