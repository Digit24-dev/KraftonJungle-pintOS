#include <stdint.h>
#include "threads/fixed_point.h"

/* Convert n to fixed point */
int itofp(int n) {
//	int floater = (1 << 14);
	return n * (1 << 14);
}

/* Convert x to integer (rounding toward zero) */
int fptoi(int x) {
//	int floater = (1 << 14);
	return x / (1 << 14);
}

/* Convert x to integer (rounding to nearest) */
int fptoi_r(int x) {
//	int mask = 0x80000000;
//	int floater = (1 << 14);

	if (x & 0x80000000) {		// <= 0
		return (x + (1 << 14)/2)/(1 << 14);
	}
	else {				// >= 0
		return (x - (1 << 14)/2)/(1 << 14);
	}
}

/* Add (x + y) */
int fp_add(int x, int y) {
	return x + y;
}

/* Subtract (x - y) */
int fp_sub(int x, int y) {
	return x - y;
}

/* Add (x + n) */
int fp_add2(int x, int n) {
	return x + n * (1 << 14);
}

/* Subtract (x - n) */
int fp_sub2(int x, int n) {
	return x - n * (1 << 14);
}

/* Multi (x * y) -> ((int64_t)x)*y */
int fp_multi(int x, int y) {
	return ((int64_t)x) * y / (1 << 14);
}

/* Multi (x * n) */
int fp_multi2(int x, int n) {
	return x * n;
}

/* Divide (x / y) -> ((int64_t)x) * f/y */
int fp_div(int x, int y) {
	return ((int64_t)x) * (1 << 14) / y;
}

/* Divide (x / n) */
int fp_div2(int x, int n) {
	return x / n;
}
/* advanced */