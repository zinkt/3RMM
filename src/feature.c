#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "wlmalloc.c"

void *parr[100];

int main()
{
    parr[0] = wl_malloc(8);
    parr[1] = wl_malloc(2);
    parr[2] = wl_malloc(168);
    parr[3] = wl_malloc(2);
    parr[4] = wl_malloc(8);
    parr[5] = wl_malloc(8);
    parr[6] = wl_malloc(32816);
    parr[7] = wl_malloc(168);
    parr[8] = wl_malloc(7);
    parr[9] = wl_malloc(168);
    parr[10] = wl_malloc(9);
    parr[11] = wl_malloc(168);
    parr[12] = wl_malloc(168);
    parr[13] = wl_malloc(168);
    parr[14] = wl_malloc(12);
    parr[15] = wl_malloc(168);
    parr[16] = wl_malloc(168);
    parr[17] = wl_malloc(88);

    wl_free(parr[0]);

    parr[18] = wl_malloc(552);

    parr[19] = wl_malloc(15);
    wl_free(parr[19]);
    parr[19] = wl_malloc(15);
    wl_free(parr[19]);
    parr[19] = wl_malloc(15);
    wl_free(parr[19]);
    parr[19] = wl_malloc(15);
    wl_free(parr[19]);
    parr[19] = wl_malloc(15);
    wl_free(parr[19]);
    wl_free(parr[10]);
    wl_free(parr[8]);
    wl_free(parr[6]);
    wl_free(parr[4]);
    wl_free(parr[2]);
    wl_free(parr[1]);
    wl_free(parr[3]);
    wl_free(parr[5]);
    wl_free(parr[7]);
    wl_free(parr[9]);
    wl_free(parr[11]);
    wl_free(parr[12]);
    wl_free(parr[17]);
    wl_free(parr[16]);
    wl_free(parr[15]);
    wl_free(parr[13]);
    wl_free(parr[14]);


    return 0;
}