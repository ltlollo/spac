#define _GNU_SOURCE
#include <sched.h>
#include <pthread.h>
#include <stdio.h>



int main () {
    pthread_t t = pthread_self();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    int rc = pthread_setaffinity_np(t, sizeof(cpu_set_t), &cpuset);
    printf("%d\n", rc);

}
