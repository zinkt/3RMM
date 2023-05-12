#include <stdio.h>
#include "../src/wlmalloc.h"

int main() {

    printf("------ start tri_mod test ------\n");

    uint32_t src_int = 0xffff;
    uint32_t *test_int = (uint32_t *)tri_mod_alloc(sizeof(uint32_t));
    printf("Before tri_mod_write: data, copy1, copy2 = %x, %x, %x\n", *test_int, *(test_int+1), *(test_int+2));
    tri_mod_write(test_int, &src_int, sizeof(uint32_t));
    printf("After tri_mod_write: data, copy1, copy2 = %x, %x, %x\n", *test_int, *(test_int+1), *(test_int+2));

    // 模拟单粒子翻转
    uint32_t mask = 0xfffffeff;
    *test_int &= mask;
    printf("\n");
    printf("Single event upset. data, copy1, copy2 = %x, %x, %x\n", *test_int, *(test_int+1), *(test_int+2));
    printf("\n");
    printf("Using tri_mod_read() to read and recover: test_int = %x\n", *(uint32_t *)tri_mod_read(test_int, sizeof(uint32_t)));
    printf("Now: data, copy1, copy2 = %x, %x, %x\n", *test_int, *(test_int+1), *(test_int+2));

    return 0;
}
