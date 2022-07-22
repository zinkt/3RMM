#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#ifdef WL_MALLOC
    #include "wlmalloc.h"
#endif

#define BLKSIZE 256

int g_total_times = 8000;
int g_tdnum = 16;
int g_threadtime[100];
int g_threadid[100];

void *twork(void *arg) {
    char *m;
    size_t i;
    struct timeval begin, end;

    gettimeofday(&begin, NULL);
    for (i=0; i<g_total_times; i++) {
#ifdef WL_MALLOC
        m = (char *)wl_malloc(BLKSIZE);
#else
        m = (char *)malloc(BLKSIZE);
#endif

        if (rand() % 2 == 0) {
#ifdef WL_MALLOC
        wl_free(m);
#else
        free(m);
#endif
        }
    }
    gettimeofday(&end, NULL);
    int time_in_us = (end.tv_sec - begin.tv_sec) * 1000000 + (end.tv_usec - begin.tv_usec);
    g_threadtime[*(int*)arg] = time_in_us;

    return NULL;
}


int main(int argc, char *argv[]) {
    char *m;
    pthread_t tid[1000];
    int i, rc;
    srand(time(NULL));

    if (argc < 3) {
        fprintf(stderr, "usage: ./a.out <total_time> <threan_num>\n");
        exit(-1);
    }
    g_total_times = atoi(argv[1]);
    g_tdnum = atoi(argv[2]);

    for (i=0; i<g_tdnum; i++) {
        g_threadid[i] = i;
        if (pthread_create(&tid[i], NULL, &twork, (void*)&g_threadid[i]) < 0) {
            printf("pthread_create err\n");
        }
    }

    for (i=0; i<g_tdnum; i++) {
        if (pthread_join(tid[i], NULL) < 0) {
            printf("pthread_join err\n");
        }
    }
    float time_in_us = 0;
    for (i=0; i<g_tdnum; i++){
        time_in_us += g_threadtime[i] / ((double)(g_tdnum * g_total_times));
    }
    printf("%lf", time_in_us);

    return 0;
}
