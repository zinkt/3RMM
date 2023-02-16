#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
int main(){
    void *p1 = sbrk(2 * 4096);
    void *p2 = sbrk(0);
    printf("%p\n%p\n", p1, p2);
    while(p1 != p2) {
        if(*(uint8_t *)p1 != 0) {
            printf("%d", *(uint8_t *)p1);
        }
        p1 = p1 + 1;
    }
    
}