#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#ifdef WL_MALLOC
    #include "wlmalloc.h"
#endif

int g_total_times = 8000;
int g_tdnum = 16;

void *twork(void *arg) {
    char *m;
    size_t i;

    for (i=0; i<g_total_times; i++) {
#ifdef WL_MALLOC
        m = (char *)wl_malloc(rand() % 65536);
#else
        m = (char *)malloc(rand() % 65536);
#endif

        if (rand() % 2 == 0) {
#ifdef WL_MALLOC
        wl_free(m);
#else
        free(m);
#endif
        }
    }

    return NULL;
}


int main(int argc, char *argv[]) {
    char *m;
    pthread_t tid[1000];
    int i, rc;
    srand(time(NULL));

    struct timeval begin, end;
    if (argc < 3) {
        fprintf(stderr, "usage: ./a.out <total_time> <threan_num>\n");
        exit(-1);
    }
    g_total_times = atoi(argv[1]);
    g_tdnum = atoi(argv[2]);

    gettimeofday(&begin, NULL);
    for (i=0; i<g_tdnum; i++) {
        if (pthread_create(&tid[i], NULL, &twork, NULL) < 0) {
            printf("pthread_create err\n");
        }
    }

    for (i=0; i<g_tdnum; i++) {
        if (pthread_join(tid[i], NULL) < 0) {
            printf("pthread_join err\n");
        }
    }
    gettimeofday(&end, NULL);
    int time_in_us = (end.tv_sec - begin.tv_sec) * 1000000 + (end.tv_usec - begin.tv_usec);
    printf("%lf", time_in_us / (double)(g_tdnum * g_total_times));

    return 0;
}
