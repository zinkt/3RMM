#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "wlmalloc.h"

void work()
{
    
    
}



int main()
{
    // void *ptrs[100];
    // ptrs[0] = wl_malloc(24576);
    // ptrs[1] = wl_malloc(24576);
    // ptrs[2] = wl_malloc(24576);
    // ptrs[3] = wl_malloc(24576);
    // wl_free(ptrs[1]);
    // wl_free(ptrs[0]);
    // wl_free(ptrs[2]);
    // ptrs[4] = wl_malloc(24576);

    pthread_t tid1,tid2;
    pthread_create(&tid1, NULL, &work, NULL);
    pthread_create(&tid2, NULL, &work, NULL);

    pthread_join(tid1, NULL) < 0;
    pthread_join(tid2, NULL) < 0;
    return 0;
}