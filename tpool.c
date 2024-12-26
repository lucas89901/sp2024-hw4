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

void* HandleRequest(void* arg) {
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
  }
  return NULL;
}

void* DoWork(void* arg) {
  struct tpool* tpool = (struct tpool*)arg;
  while (1) {
    // Try to pop work from `tpool->works`.
    pthread_mutex_lock(&tpool->works_mutex);
    while (tpool->works.size == 0) {
      pthread_cond_wait(&tpool->works_nonempty, &tpool->works_mutex);
    }
    struct Work* work = ListPop(&tpool->works);
    pthread_mutex_unlock(&tpool->works_mutex);

    for (int i = work->start; i < work->end; ++i) {
      int row = i / tpool->n;
      int column = i % tpool->n;
      work->c[row][column] =
          calculation(tpool->n, work->a[row], work->b[column]);
    }
    free(work);
  }
  return NULL;
}

// === tpool ===

struct tpool* tpool_init(int num_threads, int n) {
  struct tpool* tpool = malloc(sizeof(struct tpool));
  tpool->n = n;
  tpool->requests.size = 0;
  tpool->requests.head = NULL;
  tpool->requests.tail = NULL;
  tpool->works.size = 0;
  tpool->works.head = NULL;
  tpool->works.tail = NULL;

  pthread_mutex_init(&tpool->requests_mutex, NULL);
  pthread_cond_init(&tpool->requests_nonempty, NULL);
  pthread_mutex_init(&tpool->works_mutex, NULL);
  pthread_cond_init(&tpool->works_nonempty, NULL);

  // Frontend thread.
  pthread_create(&tpool->frontend, NULL, HandleRequest, tpool);
  // Backend threads.
  for (int i = 0; i < num_threads; ++i) {
    pthread_t thread;
    pthread_create(&thread, NULL, DoWork, tpool);
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
  // TODO: Properly sync threads.
  sleep(2);
}

void tpool_destroy(struct tpool* pool) {
  // TODO: Ensure no memory leaks.
  free(pool);
}
