#include "tpool.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

// === Queue opeartions ===

// Push back
struct ListNode* ListPush(struct List* list, void* data) {
  ++list->size;
  struct ListNode* node = malloc(sizeof(struct ListNode));
  node->data = data;
  node->next = NULL;
  if (list->tail == NULL) {
    CHECK(list->head == NULL);
    list->head = node;
    list->tail = node;
    return node;
  }
  list->tail->next = node;
  list->tail = node;
  return node;
}

// Pop front.
void* ListPop(struct List* list) {
  --list->size;
  void* data = list->head->data;
  if (list->head == list->tail) {
    CHECK(list->size == 0);
    CHECK(list->head->next == NULL);
    list->tail = NULL;
  }
  struct ListNode* remove = list->head;
  list->head = list->head->next;
  free(remove);
  return data;
}

void PrintList(struct List* list) {
  for (struct ListNode* node = list->head; node; node = node->next) {
    LOG("node->data=%p", (void*)node->data);
  }
}

// === thread ===

// Cleanup handlers ensure mutexes are unlocked after a thread is cancelled, so
// that other threads can get the mutex needed to recover from
// `pthread_cond_wait()`.
//
// According to the specification,
// > It is guaranteed that there will be at least one `tpool_synchronize` call,
// > and there will be no `tpool_request` between the last `tpool_synchronize`
// > and `tpool_destroy`.
// so both frontend and backend threads are assumed to be blocked by
// `pthread_cond_wait()` and therefore holding a mutex when `tpool_destroy()` is
// called.
//
// clang-format off
// Reference: https://man7.org/linux/man-pages/man3/pthread_cond_init.3.html#CANCELLATION
// clang-format on

void FrontendCleanup(void* arg) {
  struct tpool* tpool = (struct tpool*)arg;
  pthread_mutex_unlock(&tpool->requests_mutex);
}

void* HandleRequest(void* arg) {
  pthread_cleanup_push(FrontendCleanup, arg);
  struct tpool* tpool = (struct tpool*)arg;
  while (1) {
    // Try to pop request from `tpool->requests`.
    pthread_mutex_lock(&tpool->requests_mutex);
    while (tpool->requests.size == 0) {
      LOG("HandleRequest: Waiting for request...");
      pthread_cond_wait(&tpool->requests_nonempty, &tpool->requests_mutex);
    }
    LOG("&tpool->requests=...");
    PrintList(&tpool->requests);
    struct Request* request = ListPop(&tpool->requests);

    pthread_mutex_lock(&tpool->running_mutex);
    ++tpool->running_count;
    pthread_mutex_unlock(&tpool->running_mutex);
    pthread_mutex_unlock(&tpool->requests_mutex);
    LOG("HandleRequest: Popped request=%p", (void*)request);

    // Tranpose `b`.
    Matrix bT = malloc(tpool->n * sizeof(Vector));
    for (int i = 0; i < tpool->n; ++i) {
      bT[i] = malloc(tpool->n * sizeof(int));
      for (int j = 0; j < tpool->n; ++j) {
        bT[i][j] = request->b[j][i];
      }
    }
    for (int i = 0; i < tpool->n; ++i) {
      memcpy(request->b[i], bT[i], tpool->n * sizeof(int));
      free(bT[i]);
    }
    free(bT);

    // Split request into works.
    pthread_mutex_lock(&tpool->works_mutex);
    int i = 0, start = 0;
    int entry_count = tpool->n * tpool->n;
    int work_size = entry_count / request->num_works;
    while (start < entry_count) {
      LOG("start=%d", start);
      struct Work* work = malloc(sizeof(struct Work));
      work->a = request->a;
      work->b = request->b;
      work->c = request->c;

      work->start = start;
      work->end = start + work_size;
      if (i < entry_count % work_size) {
        ++work->end;
      }
      ++i;
      start = work->end;
      LOG("Push work (a=%p, start=%d, end=%d) to queue", (void*)work->a,
          work->start, work->end);
      ListPush(&tpool->works, work);
    }
    pthread_cond_signal(&tpool->works_nonempty);
    pthread_mutex_unlock(&tpool->works_mutex);
    free(request);

    pthread_mutex_lock(&tpool->running_mutex);
    --tpool->running_count;
    pthread_cond_signal(&tpool->done);
    pthread_mutex_unlock(&tpool->running_mutex);
  }
  pthread_cleanup_pop(0);
  return NULL;
}

void BackendCleanup(void* arg) {
  struct tpool* tpool = (struct tpool*)arg;
  pthread_mutex_unlock(&tpool->works_mutex);
}

void* DoWork(void* arg) {
  pthread_cleanup_push(BackendCleanup, arg);
  struct tpool* tpool = (struct tpool*)arg;
  while (1) {
    // Try to pop work from `tpool->works`.
    pthread_mutex_lock(&tpool->works_mutex);
    while (tpool->works.size == 0) {
      pthread_cond_wait(&tpool->works_nonempty, &tpool->works_mutex);
    }
    struct Work* work = ListPop(&tpool->works);

    pthread_mutex_lock(&tpool->running_mutex);
    ++tpool->running_count;
    pthread_mutex_unlock(&tpool->running_mutex);
    pthread_mutex_unlock(&tpool->works_mutex);
    LOG("DoWork: Doing work %p", (void*)work);

    for (int i = work->start; i < work->end; ++i) {
      int row = i / tpool->n;
      int column = i % tpool->n;
      work->c[row][column] =
          calculation(tpool->n, work->a[row], work->b[column]);
    }
    free(work);

    pthread_mutex_lock(&tpool->running_mutex);
    --tpool->running_count;
    pthread_cond_signal(&tpool->done);
    pthread_mutex_unlock(&tpool->running_mutex);
    // LOG("DoWork: Done work %p", (void*)work);
  }
  pthread_cleanup_pop(0);
  return NULL;
}

// === tpool ===

struct tpool* tpool_init(int num_threads, int n) {
  struct tpool* tpool = malloc(sizeof(struct tpool));
  tpool->n = n;
  tpool->backend_count = num_threads;
  tpool->backends = malloc(num_threads * sizeof(pthread_t));
  tpool->running_count = 0;

  tpool->requests.size = 0;
  tpool->requests.head = NULL;
  tpool->requests.tail = NULL;
  tpool->works.size = 0;
  tpool->works.head = NULL;
  tpool->works.tail = NULL;

  pthread_mutex_init(&tpool->running_mutex, NULL);
  pthread_cond_init(&tpool->done, NULL);
  pthread_mutex_init(&tpool->requests_mutex, NULL);
  pthread_cond_init(&tpool->requests_nonempty, NULL);
  pthread_mutex_init(&tpool->works_mutex, NULL);
  pthread_cond_init(&tpool->works_nonempty, NULL);

  // Frontend thread.
  pthread_create(&tpool->frontend, NULL, HandleRequest, tpool);
  // Backend threads.
  for (int i = 0; i < num_threads; ++i) {
    pthread_create(&tpool->backends[i], NULL, DoWork, tpool);
  }
  return tpool;
}

void tpool_request(struct tpool* pool, Matrix a, Matrix b, Matrix c,
                   int num_works) {
  struct Request* request = malloc(sizeof(struct Request));
  request->a = a;
  request->b = b;
  request->c = c;
  request->num_works = num_works;

  pthread_mutex_lock(&pool->requests_mutex);
  ListPush(&pool->requests, request);
  LOG("tpool_request: Push request %p, pool->requests.size=%d", (void*)request,
      pool->requests.size);
  pthread_cond_signal(&pool->requests_nonempty);
  pthread_mutex_unlock(&pool->requests_mutex);
}

void tpool_synchronize(struct tpool* pool) {
  pthread_mutex_lock(&pool->running_mutex);
  // There are still threads doing stuff.
  while (pool->requests.size > 0 || pool->works.size > 0 ||
         pool->running_count > 0) {
    LOG("tpool_synchronize: Waiting..., requets.size=%d, works.size=%d, "
        "running_count=%d",
        pool->requests.size, pool->works.size, pool->running_count);
    pthread_cond_wait(&pool->done, &pool->running_mutex);
  }
  pthread_mutex_unlock(&pool->running_mutex);
}

void tpool_destroy(struct tpool* pool) {
  pthread_cancel(pool->frontend);
  LOG("tpool_destroy: Frontend cancelled");
  for (int i = 0; i < pool->backend_count; ++i) {
    pthread_cancel(pool->backends[i]);
    LOG("tpool_destroy(): Backend %d cancalled", i);
  }
  free(pool->backends);

  pthread_mutex_destroy(&pool->running_mutex);
  pthread_cond_destroy(&pool->done);
  pthread_mutex_destroy(&pool->requests_mutex);
  pthread_cond_destroy(&pool->requests_nonempty);
  pthread_mutex_destroy(&pool->works_mutex);
  pthread_cond_destroy(&pool->works_nonempty);

  free(pool);
}
