#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include "queue.h"

struct queue {
    void **bound;
    int in;
    int out;
    int size;
    sem_t spots_left;
    sem_t spots_filled;
    sem_t mutex;
};

queue_t *queue_new(int size) {
    queue_t *q;

    // malloc the queue object and error check
    q = (queue_t *) malloc(sizeof(queue_t));
    if (q == NULL) {
        return NULL;
    }

    // malloc the bounded buffer of length size and error check
    q->bound = (void **) malloc(sizeof(void *) * size);
    if (q->bound == NULL) {
        // if error free queue object q and return
        free(q);
        q = NULL;
        return NULL;
    }

    // initialize static values
    q->in = 0;
    q->out = 0;
    q->size = size;

    // initialize semaphores and error check
    if (sem_init(&(q->spots_left), 0, size)
        != 0) { // size resources (spots left) at the start (counting)
        fprintf(stderr, "sem_init() failed for sempaphore: spots left");
        return NULL;
    }
    if (sem_init(&(q->spots_filled), 0, 0)
        != 0) { // 0 resources (spots filled) at the start (counting)
        fprintf(stderr, "sem_init() failed for sempaphore: spots filled");
        return NULL;
    }
    if (sem_init(&(q->mutex), 0, 1) != 0) { // mutex starts at 1 (binary =)
        fprintf(stderr, "sem_init() failed for sempaphore: mutex");
        return NULL;
    }

    return q;
}

void queue_delete(queue_t **q) {
    if (q != NULL && *q != NULL) {
        // free the bounded buffer
        free((*q)->bound);
        (*q)->bound = NULL;

        // free the semaphors
        if (sem_destroy(&(*q)->spots_left)) {
            fprintf(stderr, "sem_destroy() failed for sempaphore: spots left");
        }
        if (sem_destroy(&(*q)->spots_filled)) {
            fprintf(stderr, "sem_destroy() failed for sempaphore: spots filled");
        }
        if (sem_destroy(&(*q)->mutex)) {
            fprintf(stderr, "sem_destroy() failed for sempaphore: mutex");
        }

        free(*q);
        *q = NULL;
    }
}

bool queue_push(queue_t *q, void *elem) {
    if (q == NULL) {
        return false;
    }
    // spots left down
    sem_wait(&(q->spots_left));
    // mutex down
    sem_wait(&(q->mutex));
    // Buffer[in] = item
    q->bound[q->in] = elem;
    // in = (in + 1) % N
    q->in = (q->in + 1) % (q->size);
    // mutex up
    sem_post(&(q->mutex));
    // spots filled up
    sem_post(&(q->spots_filled));
    return true;
}

bool queue_pop(queue_t *q, void **elem) {
    if (q == NULL) {
        return false;
    }
    // spots filled down
    sem_wait(&(q->spots_filled));
    // mutex down
    sem_wait(&(q->mutex));
    // Buffer[in] = item
    *elem = q->bound[q->out];
    // in = (in + 1) % N
    q->out = (q->out + 1) % (q->size);
    // mutex up
    sem_post(&(q->mutex));
    // spots filled up
    sem_post(&(q->spots_left));
    return true;
}
