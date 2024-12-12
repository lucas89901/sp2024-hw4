#pragma once

#include <pthread.h>

/* You may define additional structures / typedef's in this header file if
 * needed.
 */

struct tpool {
  // Add things you need here
};

typedef int** Matrix;
typedef int* Vector;

struct tpool* tpool_init(int num_threads, int n);
void tpool_request(struct tpool*, Matrix a, Matrix b, Matrix c, int num_works);
void tpool_synchronize(struct tpool*);
void tpool_destroy(struct tpool*);
int calculation(int n, Vector, Vector);  // Already implemented
