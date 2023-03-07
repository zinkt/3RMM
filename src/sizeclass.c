#include <stdio.h>
int cls2size[64];
char size2cls8[128];
char size2cls512[128];

void gen_cls2size() {
    int curcls;
    int cursize;

    // 跨度1 8byte:  size <= 128
    curcls = 0;
    for(cursize = 16; cursize <= 128; cursize += 8) {
        cls2size[curcls++] = cursize;
    }
    // 跨度2 : 128 < size <= 64KB
    // an+1 = 1.5an
    // an+2 = 2an
    // 但是注意，1.5an这个size不会再继续作为an
    for(cursize = 128; cursize < 1 << 16; cursize <<= 1) {
        cls2size[curcls++] = cursize + (cursize >> 1);
        cls2size[curcls++] = cursize << 1;
    }
}

void gen_size2cls() {
    int curcls;
    int cursize;
    // 跨度1表
    curcls = 0;
    for(cursize = 16; cursize <= 1024; cursize += 8) {
        if(cursize > cls2size[curcls]) {
            curcls++;
        }
        size2cls8[(cursize-1) >> 3] = curcls;
    }
    // 跨度2表
    for(cursize = 1024; cursize <= 65536; cursize += 512) {
        if(cursize > cls2size[curcls]) {
            curcls++;
        }
        size2cls512[(cursize-1) >> 9] = curcls;
    }

}
void show() {
    printf("cls2size:\n");
    for(int i = 0; i < 64; i++) {
        printf("%d, ", cls2size[i]);
    }
    printf("\nsize2cls:8\n");
    for(int i = 0; i < 128; i++) {
        printf("%d, ", size2cls8[i]);
    }
    printf("\nsize2cls512:\n");
    for(int i = 0; i < 128; i++) {
        printf("%d, ", size2cls512[i]);
    }
}

int main() {
    gen_cls2size();
    gen_size2cls();
    show();
    return 0;
}