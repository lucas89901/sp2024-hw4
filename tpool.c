#include "tpool.h"

#include <pthread.h>

struct tpool *tpool_init(int num_threads, int n) {
}

void tpool_request(struct tpool *pool, Matrix a, Matrix b, Matrix c,
                   int num_works) {
}

void tpool_synchronize(struct tpool *pool) {
}

void tpool_destroy(struct tpool *pool) {
}
