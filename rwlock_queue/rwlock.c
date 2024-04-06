#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>

#include "rwlock.h"

struct rwlock {
    PRIORITY p;
    uint32_t n;

    int r_active;
    int w_active;

    int r_waiting;
    int w_waiting;

    uint32_t r_count;

    pthread_mutex_t mutex;
    pthread_cond_t read_ok;
    pthread_cond_t write_ok;
};

rwlock_t *rwlock_new(PRIORITY p, uint32_t n) {
    /*
    initialize semaphores
    */
    rwlock_t *rw;
    rw = (rwlock_t *) malloc(sizeof(rwlock_t));

    if (p == N_WAY && n <= 0) {
        fprintf(stderr, "invalid n for N_WAYS\n");
        exit(1);
    }

    rw->n = n;
    rw->p = p;

    rw->r_active = 0;
    rw->w_active = 0;

    rw->r_waiting = 0;
    rw->w_waiting = 0;

    rw->r_count = 0;

    if (pthread_mutex_init(&rw->mutex, NULL) != 0) {
        return NULL;
    }
    if (pthread_cond_init(&rw->read_ok, NULL) != 0) {
        return NULL;
    }
    if (pthread_cond_init(&rw->write_ok, NULL) != 0) {
        return NULL;
    }

    return rw;
}

void rwlock_delete(rwlock_t **rw) {

    pthread_mutex_destroy(&(*rw)->mutex);
    pthread_cond_destroy(&(*rw)->read_ok);
    pthread_cond_destroy(&(*rw)->write_ok);

    free(*rw);
    rw = NULL;
}

void reader_lock(rwlock_t *rw) {
    printf("* reader_lock(): entered\n");

    pthread_mutex_lock(&(rw->mutex));

    printf("reader_lock(): mutex down\n");

    if (rw->p == WRITERS) {
        rw->r_waiting += 1;
        printf("reader_lock(): WRITERS clause\n");
        while (rw->w_active > 0 || rw->w_waiting > 0) {
            pthread_cond_wait(&rw->read_ok, &rw->mutex);
        }
        rw->r_waiting -= 1;
        rw->r_active += 1;
    }

    else if (rw->p == READERS) {
        rw->r_waiting += 1;
        printf("reader_lock(): READERS clause\n");
        while (rw->w_active > 0) {
            pthread_cond_wait(&(rw->read_ok), &(rw->mutex));
        }
        rw->r_waiting -= 1;
        rw->r_active += 1;
    }

    else {
        rw->r_waiting += 1;
        printf("reader_lock(): N_WAYS clause\n");
        while ((rw->r_count >= rw->n && rw->w_waiting > 0) || rw->w_active > 0) {
            printf("reader_lock(): N_WAYS clause while before");
            pthread_cond_wait(&(rw->read_ok), &(rw->mutex));
            printf("reader_lock(): N_WAYS clause while after");
        }
        rw->r_waiting -= 1;
        rw->r_active += 1;
        rw->r_count += 1;
    }
    pthread_mutex_unlock(&rw->mutex);
    printf("reader_lock(): mutex up\n");
    printf("reader_lock(): exiting\n");
}

void reader_unlock(rwlock_t *rw) {
    printf("- reader_unlock(): entered\n");

    pthread_mutex_lock(&rw->mutex);
    printf("reader_unlock(): mutex down\n");

    rw->r_active -= 1;

    if (rw->p == WRITERS) {
        printf("reader_unlock(): WRITERS clause\n");
        if (rw->r_active == 0 && rw->w_waiting > 0) {
            pthread_cond_signal(&(rw->write_ok));
            printf("reader_unlock(): WRITERS clause write_ok signaled\n");
        }
    } else if (rw->p == READERS) {
        printf("reader_unlock(): READERS clause\n");
        if (rw->r_active == 0 && rw->r_waiting == 0 && rw->w_waiting > 0) {
            pthread_cond_signal(&(rw->write_ok));
            printf("reader_unlock(): READERS clause write_ok signaled\n");
        }
    } else {
        printf("reader_unlock(): N_WAYS clause\n");
        if (rw->r_count < rw->n && rw->r_waiting > 0) {
            pthread_cond_broadcast(&(rw->read_ok));
            printf("reader_unlock(): N_WAYS clause read_ok signaled\n");
        } else if ((rw->r_count >= rw->n && rw->w_waiting > 0)
                   || (rw->w_waiting > 0 && rw->r_waiting == 0)) {
            pthread_cond_signal(&(rw->write_ok));
            printf("reader_unlock(): N_WAYS clause write_ok signaled\n");
        }
    }
    printf("reader_unlock(): exited\n");
    pthread_mutex_unlock(&(rw->mutex));
}

void writer_lock(rwlock_t *rw) {
    printf("\t* writer_lock(): entered\n");
    if (pthread_mutex_lock(&(rw->mutex))) {
        return;
    }
    printf("\twriter_lock(): mutex down\n");

    rw->w_waiting += 1;

    if (rw->p == WRITERS) {
        printf("\twriter_lock(): WRITERS clause\n");
        while (rw->w_active > 0 || rw->r_active > 0) {
            printf("\twriter_lock(): WRITERS clause while\n");
            pthread_cond_wait(&(rw->write_ok), &(rw->mutex));
        }
        rw->w_waiting -= 1;
        rw->w_active += 1;
    }

    else if (rw->p == READERS) {
        printf("\twriter_lock(): READERS clause\n");
        while (rw->r_active > 0 || rw->w_active > 0 || rw->r_waiting > 0) {
            printf("\twriter_lock(): WRITERS clause while\n");
            pthread_cond_wait(&(rw->write_ok), &(rw->mutex));
        }
        rw->w_waiting -= 1;
        rw->w_active += 1;
    }

    else {
        printf("\twriter_lock(): N_WAYS clause\n");
        while ((rw->r_count < rw->n && rw->r_waiting > 0) || rw->w_active || rw->r_active) {
            printf("\twriter_lock(): N_WAYS clause while before\n");
            pthread_cond_wait(&(rw->write_ok), &(rw->mutex));
            printf(
                "\twriter_lock(): N_WAYS clause while after %d %d \n", rw->r_count, rw->r_active);
        }
        rw->r_count = 0;
        rw->w_waiting -= 1;
        rw->w_active += 1;
    }
    pthread_mutex_unlock(&(rw->mutex));
    printf("\twriter_lock(): mutex up\n");
    printf("\twriter_lock(): exited\n");
}

void writer_unlock(rwlock_t *rw) {
    printf("- writer_unlock(): entered\n");
    if (pthread_mutex_lock(&(rw->mutex))) {
        return;
    }
    printf("writer_unlock(): mutex down\n");

    rw->w_active -= 1;

    if (rw->p == WRITERS) {
        printf("writer_unlock(): WRITERS clause\n");
        if (rw->w_waiting > 0) {
            pthread_cond_signal(&(rw->write_ok));
            printf("writer_unlock(): WRITERS clause write_ok signaled\n");
        } else if (rw->r_waiting > 0) {
            pthread_cond_broadcast(&(rw->read_ok));
            printf("writer_unlock(): WRITERS clause read_ok signaled\n");
        }
    }

    else if (rw->p == READERS) {
        printf("writer_unlock(): READERS clause\n");
        if (rw->r_waiting > 0) {
            pthread_cond_broadcast(&(rw->read_ok));
            printf("writer_unlock(): WRITERS clause read_ok broadcasted\n");
        } else if (rw->w_waiting > 0) {
            pthread_cond_signal(&(rw->write_ok));
            printf("writer_unlock(): READERS clause write_ok signaled\n");
        }
    }

    else {
        printf("writer_unlock(): N_WAYS clause\n");
        if (rw->r_count < rw->n && rw->r_waiting > 0) {
            pthread_cond_signal(&(rw->read_ok));
            printf("writer_unlock(): N_WAYS clause write_ok signaled\n");
        } else if (rw->w_waiting > 0 && rw->r_waiting == 0) {
            pthread_cond_signal(&(rw->write_ok));
            printf("writer_unlock(): N_WAYS clause write_ok signaled\n");
        }
    }
    printf("writer_unlock(): exited\n");
    pthread_mutex_unlock(&(rw->mutex));
}
