#ifndef WL_GC_H
#define WL_GC_H

#include <stdint.h>
#include <sys/types.h>

#define MIN_CAPACITY        1024    // 最小哈希表大小
#define DEFAULT_CAPACITY    1024    // 默认初始hash表大小
#define DOWNSIZE_FACTOR     0.2     // 高水位
#define UPSIZE_FACTOR       0.8     // 低水位
#define SWEEP_FACTOR        0.5     // 清除水位


// static uint8_t *__rbp;
// #define __READ_RBP() __asm__ volatile("movq %%rbp, %0" : "=r"(__rbp))
static uint8_t *__rsp;
#define __READ_RSP() __asm__ volatile("movq %%rsp, %0" : "=r"(__rsp))

#define ROUNDUP(x,n)        (((x)+(n)-1)/(n)*(n))


typedef struct alloc_s alloc_t;
typedef struct gc_s gc_t;

struct alloc_s
{
    void *p;                    // 指向分配内存的指针
    size_t size_tag;            // 该段空间的大小，以及标记-清除的tag
    void (*destructor)(void*);  // destructor func
    alloc_t *next;              // 链
};

struct gc_s
{
    void *bos;                  // bottom of stack 栈底

    // 记录已分配的内存
    alloc_t* allocs[DEFAULT_CAPACITY];  // 已分配 哈希表
    size_t size;                        // 已分配alloc数
    size_t sweep_limit;
};

// Jenkin's 32 bit hash function (Guarantee's a good hash)
int hash(void *key) {
	long a = (long)key;
	a = (a + 0x7ed55d16) + (a << 12);
	a = (a ^ 0xc761c23c) ^ (a >> 19);
	a = (a + 0x165667b1) + (a << 5);
	a = (a + 0xd3a2646c) ^ (a << 9);
	a = (a + 0xfd7046c5) + (a << 3);
	a = (a ^ 0xb55a4f09) ^ (a >> 16);
	return (int)a;
}

void gc_init();
void gc_sweep();
void gc_collect();
void *gc_malloc(size_t size);
void gc_free(void *ptr);
#endif              // WL_GC_H