#include "wlalloc.h"

void mmap_test()
{
    int *ptr = (int *)mmap(NULL, 32, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    *ptr = 16;

    getchar();
}
int main()
{
    mmap_test();


    return 0;
}