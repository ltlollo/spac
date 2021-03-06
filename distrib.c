#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdatomic.h>
#include "splib.h"

#define ACQUIRE memory_order_acquire
#define RELEASE memory_order_release
#define RELAX memory_order_relaxed

typedef struct {
    _Atomic(u8 *) stacks[4];
    FILE **files;
    size_t fsize;
    u8 n;
    _Atomic(int) err;
    FILE* out;
} Shared;

typedef struct {
    _Atomic(size_t) ready;
    u8 cpu;
    u8 nt;
    u8 *stack;
    Shared *pub;
} Private;

static void xorv(u8 *restrict f, u8 *restrict s);
static int cpu_pin(Private *pri);
static void *master(void *arg);
static void *slave(void *arg);
static void *reduce(void *arg);

static void xorv(u8 *restrict f, u8 *restrict s) {
    for (unsigned i = 0; i < BUFSIZE; ++i) {
        f[i] ^= s[i];
    }
}

static int cpu_pin(Private *pri) {
    pthread_t t = pthread_self();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(pri->cpu, &cpuset);
    return pthread_setaffinity_np(t, sizeof(cpu_set_t), &cpuset);
}

static void *master(void *arg) {
    u8 stack[BUFSIZE];
    Private *pri = (Private *)arg;
    cpu_pin(pri);
    pri->stack = stack;
    size_t size = pri->pub->fsize;
    u8 n = pri->pub->n;
    FILE **files = pri->pub->files;
    u8 cpu = pri->cpu;
    while (size) {
        size_t chunksize = size < BUFSIZE ? size : BUFSIZE;
        u8 i = 0;
        do {
            if (fread(stack, 1, chunksize, files[i]) != chunksize) {
                atomic_store_explicit(&pri->pub->err, -1, RELAX);
            }
            if (i+1 < n) {
                while (!atomic_load_explicit(&(pri+1)->ready, ACQUIRE)) {
                }
                xorv(stack, (pri+1)->stack);
            }
            i += 8;
        } while (i < n);
        atomic_store_explicit(&pri->pub->stacks[cpu], stack, RELEASE);
        while (!atomic_load_explicit(&pri->pub->stacks[0], ACQUIRE)) {
        }
        size -= chunksize;
    }
    return NULL;
}

static void *slave(void *arg) {
    u8 stack[BUFSIZE];
    Private *pri = (Private *)arg;
    cpu_pin(pri);
    pri->stack = stack;
    size_t size = pri->pub->fsize;
    u8 n = pri->pub->n;
    FILE **files = pri->pub->files;
    while (size) {
        size_t chunksize = size < BUFSIZE ? size : BUFSIZE;
        u8 i = 0;
        do {
            if (fread(stack, 1, chunksize, files[i]) != chunksize) {
                atomic_store_explicit(&pri->pub->err, -1, RELAX);
            }
            atomic_store_explicit(&pri->ready, 1, RELEASE);
            i += 8;
        } while (i < n);
        size -= chunksize;
    }
    return NULL;
}


static void *reduce(void *arg) {
    u8 stack[BUFSIZE];
    Private *pri = (Private *)arg;
    cpu_pin(pri);
    pri->stack = stack;
    size_t size = pri->pub->fsize;
    u8 n = pri->pub->n;
    FILE **files = pri->pub->files;
    FILE *out = pri->pub->out;
    while (size) {
        size_t chunksize = size < BUFSIZE ? size : BUFSIZE;
        u8 i = 0;
        do {
            if (fread(stack, 1, chunksize, files[i]) != chunksize) {
                atomic_store_explicit(&pri->pub->err, -1, RELAX);
            }
            if (i+1 < n) {
                while (!atomic_load_explicit(&(pri+1)->ready, ACQUIRE)) {
                }
                xorv(stack, (pri+1)->stack);
            }
            i += 8;
        } while (i < n);
        for (i = 1; i < n/2; ++i) {
            while (!atomic_load_explicit(&pri->pub->stacks[i], ACQUIRE)) {
            }
            xorv(stack, (pri+2*i)->stack);
        }
        if (fwrite(stack, 1, chunksize, out) != chunksize) {
            atomic_store_explicit(&pri->pub->err, -1, RELAX);
        }
        atomic_store_explicit(&pri->pub->stacks[0], stack, RELEASE);
        size -= chunksize;
    }
    return NULL;
}

int spawn(size_t fsize, FILE *files[], u8 n, FILE *out) {
    pthread_t ts[4];
    Shared pub = {
        .files  = files,
        .n      = n,
        .stacks = {NULL},
        .fsize  = fsize,
        .err    = 0,
        .out    = out,
    };
    Private thrd[8];
    for (u8 i = 0; i < 8 && i < n; ++i) {
        thrd[i].ready = 0;
        thrd[i].cpu   = i/2;
        thrd[i].nt    = i%2;
        thrd[i].pub   = &pub;
        if (i > 0) {
            if (i % 2) {
                if (pthread_create(ts, NULL, master, &thrd[i])) {
                    return -1;
                }
            } else {
                if (pthread_create(ts, NULL, slave, &thrd[i])) {
                    return -1;
                }
            }
        }
    }
    reduce(&thrd[0]);
    return atomic_load_explicit(&pub.err, ACQUIRE);
}
