#include "log.h"
#include "wlgc.h"
void func() {
    int *arr = gc_malloc(1024 * sizeof(int));
    for(int i = 0; i < 1024; i++) {
        arr[i] = i;
    }
}

int main(int argc, char *argv[]){
    gc_init();
    func();

    return 0;
}
