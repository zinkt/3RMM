#include "wlalloc.h"

void mmap_test()
{
    int *ptr = (int *)mmap(NULL, 32, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    *ptr = 16;

    getchar();
}
void sbrk_test()
{
    int *ptr = (int *)sbrk(8);
    *ptr = 88;
    ptr = (int *)sbrk(16);
    getchar();
}
struct block
{
    uint32_t val;
    list_head list;
};
typedef struct block block;

list_head head;
int main()
{

    INIT_LIST_HEAD(&head);
    
    block b;
    b.val = 77;
    
    list_add(&b.list, &head);
    uint32_t val =  list_entry(head.next, block, list)->val;
    

    return 0;
}