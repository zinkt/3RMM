//wlmalloc正确性测试：wl_malloc, wl_free
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "wlmalloc.h"
void test1();
void test2();
void test3();
void test4();

// 用于getopt()
extern char* optarg;
extern int optind;
extern int opterr;
extern int optopt;

int main(int argc, char *argv[])
{
    int ch;
    //命令行参数处理
    while ((ch = getopt(argc, argv, "at:"))!=-1)
    {
        switch (ch)
        {
        case 't':
            switch (atoi(optarg))
            {
            case 1:
                test1();
                break;
            case 2:
                test2();
                break;
            case 3:
                test3();
                break;
            case 4:
                test4();
                break;
            
            default:
                printf("No such test!\n");
                break;
            }
            break;
        case 'a':
            test1();
            test2();
            test3();
            test4();
            break;
        default:
            printf("Usage: %s [-a] [-t testnum]\n", argv[0]);
            break;
        }
    }
    return 0;
}
// 测试单次malloc
void test1()
{
    printf("----------start test 1----------\n");  
    printf("测试单次malloc\n");
    void *addr = wl_malloc(20 * sizeof(int));
    printf("wl_malloc(20 * sizeof(int)) addr: %p\n", addr);
    printf("-----------end test 1\n");  
}
// 测试单次malloc和free
void test2()
{
    printf("----------start test 2----------\n");  
    printf("测试单次malloc和free\n");
    void *addr = wl_malloc(20 * sizeof(int));
    printf("wl_malloc(20 * sizeof(int)) addr: %p\n", addr);
    printf("释放空间\n");
    wl_free(addr);
    printf("-----------end test 2\n");  
}
// 测试连续malloc不同大小的空间
void test3()
{
    printf("----------start test 3----------\n");  
    printf("测试连续malloc不同大小的空间\n");
    void *addr;
    addr = wl_malloc(0);
    printf("wl_malloc(0)        addr: %p\n", addr);
    addr = wl_malloc(2);
    printf("wl_malloc(2)        addr: %p\n", addr);
    addr = wl_malloc(20);
    printf("wl_malloc(20)       addr: %p\n", addr);
    addr = wl_malloc(200);
    printf("wl_malloc(200)      addr: %p\n", addr);
    addr = wl_malloc(2000);
    printf("wl_malloc(2000)     addr: %p\n", addr);
    addr = wl_malloc(20000);
    printf("wl_malloc(20000)    addr: %p\n", addr);
    addr = wl_malloc(200000);
    printf("wl_malloc(200000)   addr: %p\n", addr);
    printf("-----------end test 3\n");  
}
// 测试多次malloc和free
void test4()
{
    printf("----------start test 4----------\n");  
    printf("测试多次malloc和free\n");
    
    void *addr;
    int64_t mount_addr;
    int64_t span_addr;
    addr = wl_malloc(100000);
    wl_free(addr);
    mount_addr = (int64_t)addr >> 20;
    addr = wl_malloc(30000);
    wl_free(addr);
    span_addr = (int64_t)addr >> 20;
    assert(span_addr != mount_addr);
    addr = wl_malloc(0);
    wl_free(addr);
    assert(span_addr == (int64_t)addr>>20);
    addr = wl_malloc(1);
    wl_free(addr);
    assert(span_addr == (int64_t)addr>>20);
    addr = wl_malloc(10);
    wl_free(addr);
    assert(span_addr == (int64_t)addr>>20);
    addr = wl_malloc(100);
    wl_free(addr);
    assert(span_addr == (int64_t)addr>>20);
    addr = wl_malloc(1000);
    wl_free(addr);
    assert(span_addr == (int64_t)addr>>20);
    printf("-----------end test 4\n");  
}
