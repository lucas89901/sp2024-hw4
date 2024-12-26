#pragma once

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#ifdef B12902110_DEBUG
#define CHECK(...) assert(__VA_ARGS__)
#define LOG(...)                                                              \
  fprintf(stderr, "[%d:%d:%s(%d)] ", getpid(), gettid(), __FILE__, __LINE__); \
  fprintf(stderr, __VA_ARGS__);                                               \
  fprintf(stderr, "\n")
#else
#define CHECK(...) __VA_ARGS__
#define LOG(...)
#endif

typedef int** Matrix;
typedef int* Vector;

/* You may define additional structures / typedef's in this header file if
 * needed.
 */

struct Work {
  Matrix a, b, c;
  // [start, end)
  int start, end;
};

struct Request {
  Matrix a, b, c;
  int num_works;
};

struct ListNode {
  void* data;
  struct ListNode* next;
};

struct List {
  int size;
  // `head` is first.
  struct ListNode* head;
  struct ListNode* tail;
};

struct tpool {
  int n;  // Size of matrices.

  pthread_t frontend;
  int backend_count;
  pthread_t* backends;

  int running_count;
  pthread_mutex_t running_mutex;
  pthread_cond_t done;

  struct List requests;
  pthread_mutex_t requests_mutex;
  pthread_cond_t requests_nonempty;

  struct List works;
  pthread_mutex_t works_mutex;
  pthread_cond_t works_nonempty;
};

struct tpool* tpool_init(int num_threads, int n);
void tpool_request(struct tpool*, Matrix a, Matrix b, Matrix c, int num_works);
void tpool_synchronize(struct tpool*);
void tpool_destroy(struct tpool*);
int calculation(int n, Vector, Vector);  // Already implemented
