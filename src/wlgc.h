#ifndef WL_GC_H
#define WL_GC_H

#include <stdint.h>
#include <sys/types.h>

#define MIN_CAPACITY        1024    // 最小哈希表大小
#define DEFAULT_CAPACITY    4096    // 默认初始hash表大小
#define LOAD_FACTOR         0.7     // hash表扩容比例
#define SWEEP_FACTOR        0.5     // 触发清除


// static uint8_t *__rbp;
// #define __READ_RBP() __asm__ volatile("movq %%rbp, %0" : "=r"(__rbp))
static uint8_t *__rsp;
#define __READ_RSP() __asm__ volatile("movq %%rsp, %0" : "=r"(__rsp))

#define ROUNDUP(x,n)        (((x)+(n)-1)/(n)*(n))


typedef struct alloc_s alloc_t;
typedef struct gc_s gc_t;

struct alloc_s{
    void *p;          // 指向分配内存的指针
    size_t size_tag;  // 该段空间的大小，以及标记-清除的tag
    alloc_t *next;    // 链
};

struct gc_s{
    void *bos;        // bottom of stack 栈底
    alloc_t** allocs; // 已分配 哈希表
    size_t capacity;  // allocs容量
    size_t size;      // 已分配alloc数
    size_t sweep_limit;
    size_t gc_cnt;
};

// 由使用者调用，将会设置调用者的首地址为gc的栈底
void gc_init();
// 手动触发垃圾回收接口
void gc_collect();
// 需要进行跟踪并回收的内存分配接口
void *gc_malloc(size_t size);
// 手动释放某个已被跟踪的内存
void gc_free(void *ptr);
// 内存分配接口
void *gc_calloc(size_t nmemb, size_t size);
void *gc_realloc(void *ptr, size_t size);
// 用于统计回收内存块数目
size_t gc_collected();

// Jenkin's 32 bit hash function (Guarantee's a good hash)
// static uint32_t hash_func(void *key) {
// 	long a = (long)key;
// 	a = (a + 0x7ed55d16) + (a << 12);
// 	a = (a ^ 0xc761c23c) ^ (a >> 19);
// 	a = (a + 0x165667b1) + (a << 5);
// 	a = (a + 0xd3a2646c) ^ (a << 9);
// 	a = (a + 0xfd7046c5) + (a << 3);
// 	a = (a ^ 0xb55a4f09) ^ (a >> 16);
// 	return (uint32_t)a;
// }

static uint64_t hash_func(uint64_t key) {
  key = (~key) + (key << 21); // key = (key << 21) - key - 1;
  key = key ^ (key >> 24);
  key = (key + (key << 3)) + (key << 8); // key * 265
  key = key ^ (key >> 14);
  key = (key + (key << 2)) + (key << 4); // key * 21
  key = key ^ (key >> 28);
  key = key + (key << 31);
  return key;
}
#endif              // WL_GC_H