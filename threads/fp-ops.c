#include "threads/fp-ops.h"
#define f	(1 << 14)

/* Convert n to fixed point */
inline fp_float itofp(int n) {
	return n * f;
}

/* Convert x to integer (rounding toward zero) */
inline int fptoi(fp_float x) {
	return x / f;
}

/* Convert x to integer (rounding to nearest) */
inline int fptoi_r(fp_float x) {
	if (x >= 0)
		return (x + f/2)/f;
	else
		return (x - f/2)/f;
}

/* Add (x + y) */
inline fp_float fp_add(fp_float x, fp_float y) {
	return x + y;
}

/* Add (x + n) */
inline fp_float fp_add2(fp_float x, int n) {
	return x + n * f;
}

/* Subtract (x - y) */
inline fp_float fp_sub(fp_float x, fp_float y) {
	return x - y;
}

/* Subtract (x - n) */
inline fp_float fp_sub2(fp_float x, int n) {
	return x - n * f;
}

/* Multi (x * y) -> ((int64_t)x)*y */
inline fp_float fp_multi(fp_float x, fp_float y) {
	return ((fp_double)x) * y / f;
}

/* Multi (x * n) */
inline fp_float fp_multi2(fp_float x, int n) {
	return x * n;
}

/* Divide (x / y) -> ((int64_t)x) * f/y */
inline fp_float fp_div(fp_float x, fp_float y) {
	return ((fp_double)x) * f / y;
}

/* Divide (x / n) */
inline fp_float fp_div2(fp_float x, int n) {
	return x / n;
}