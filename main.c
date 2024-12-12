#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "tpool.h"

#define error_exit(msg)                                     \
  _Generic((msg),                                           \
      char *: (fprintf(stderr, "%s: %s\n", argv[0], (msg)), \
               exit(EXIT_FAILURE)))

int calculation(int n, Vector a, Vector b) {
  int res = 0;
  for (int i = 0; i < n; i++) res += a[i] * b[i];
  return res;
}

int main([[maybe_unused]] int argc, char *argv[]) {
  int num_threads;
  int n, t;
  if (scanf("%d%d%d", &num_threads, &n, &t) != 3) err(EXIT_FAILURE, "scanf");
  if (num_threads <= 0 || n <= 0 || t <= 0)
    error_exit("invalid range of num_threads, n or t");

  Matrix a[t], b[t], c[t];
  auto pool = tpool_init(num_threads, n);

  for (int i = 0; i < t; i++) {
    a[i] = calloc(n, sizeof(Vector));
    b[i] = calloc(n, sizeof(Vector));
    c[i] = calloc(n, sizeof(Vector));
    if (a[i] == nullptr || b[i] == nullptr || c[i] == nullptr)
      err(EXIT_FAILURE, "malloc");
    a[i][0] = calloc(n * n, sizeof(int));
    b[i][0] = calloc(n * n, sizeof(int));
    c[i][0] = calloc(n * n, sizeof(int));
    if (a[i][0] == nullptr || b[i][0] == nullptr || c[i][0] == nullptr)
      err(EXIT_FAILURE, "malloc");
    for (int j = 1; j < n; j++) {
      a[i][j] = a[i][j - 1] + n;
      b[i][j] = b[i][j - 1] + n;
      c[i][j] = c[i][j - 1] + n;
    }

    for (int x = 0; x < n; x++)
      for (int y = 0; y < n; y++)
        if (scanf("%d", &a[i][x][y]) != 1) err(EXIT_FAILURE, "scanf");
    for (int x = 0; x < n; x++)
      for (int y = 0; y < n; y++)
        if (scanf("%d", &b[i][x][y]) != 1) err(EXIT_FAILURE, "scanf");
    for (int x = 0; x < n; x++)
      for (int y = 0; y < n; y++)
        if (a[i][x][y] < -100 || a[i][x][y] > 100 || b[i][x][y] < -100 ||
            b[i][x][y] > 100)
          error_exit("invalid range of matrix elements");
    int num_works;
    if (scanf("%d", &num_works) != 1 || num_works <= 0)
      err(EXIT_FAILURE, "scanf");
    tpool_request(pool, a[i], b[i], c[i], num_works);
  }

  tpool_synchronize(pool);
  printf("--start--\n");
  for (int i = 0; i < t; i++) {
    for (int x = 0; x < n; x++) {
      for (int y = 0; y < n; y++) {
        printf("%d", c[i][x][y]);
        if (y + 1 < n)
          putchar(' ');
        else
          putchar('\n');
      }
    }
  }

  for (int i = 0; i < t; i++) {
    free(a[i][0]);
    free(b[i][0]);
    free(c[i][0]);
    free(a[i]);
    free(b[i]);
    free(c[i]);
  }
  tpool_destroy(pool);
}
