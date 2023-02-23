#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#define PAGE 4096

int mem_init();
void *mem_alloc(uint32_t size);
void mem_free(void * bp);

int inited = 0;
void * heap_begin_p;
void * heap_end_p;

#define PACK(size, alloc)   ((size) | (alloc))

#define GET(p)      (*(uint32_t *)(p))
#define PUT(p, val) (*(uint32_t *)(p) = (val))    

// 从地址p上读出块大小和已分配位，p处于header或footer
#define GET_SIZE(p)    (GET(p) & ~0x1)
#define GET_ALLOC(p)   (GET(p) & 0x1)

// !!!!!!!!根据payload地址找到header或footer的地址
#define HDRP(p) ((char *)(p) - 4)
#define FTRP(p) ((char *)(p) + GET_SIZE(HDRP(p)) - 8)

// 由block的起始地址，找到下一块或上一块block
#define NEXT_P(p) ((char *)(p) + GET_SIZE(p))
#define PREV_P(p) ((char *)(p) - GET_SIZE(((char *)(p) - 4)))

//  Round up x to next multiple of n;
//      if (x == k * n) return x
//      else if(x = k * n + m && m < n) return (k+1) * n;
static uint64_t round_up(uint64_t x, uint64_t n) {
    return n * ((x + n - 1) / n);
}


/// @brief split a block(starting at bp) into an allocated block with blocksize equals required
///         and a free block(if remaining blocksize >= 16)
/// @param bp 
/// @param required 
static void place(void *bp, uint32_t required) {
        uint32_t bsize = GET_SIZE(bp);
        // 如果剩余block size < 16，则不必再将其标记为可用空闲块 
        if(bsize - required < 16){
            PUT(bp, PACK(bsize, 1));
            PUT(FTRP(bp + 4), PACK(bsize, 1));
            return;
        }
        // 否则将剩余的空间进行标记
        PUT(bp, PACK(required, 1));
        PUT(FTRP(bp + 4), PACK(required, 1));
        bp = NEXT_P(bp);
        PUT(bp, PACK(bsize - required, 0));
        PUT(FTRP(bp + 4), PACK(bsize - required, 0));
}

// 尝试合并bp所指向的block的前后两个block，并返回合并后的block的hdr
void *coalesce(void *bp) {
    uint32_t bsize = GET_SIZE(bp);
    uint32_t next_alloc;
    uint32_t prev_alloc;
    if(FTRP(bp+4)+4 >= (char *)heap_end_p) {
        next_alloc = 1;
    }else {
        next_alloc = GET_ALLOC(NEXT_P(bp));
    }
    if(bp <= heap_begin_p){
        prev_alloc = 1;
    }else {
        prev_alloc = GET_ALLOC(PREV_P(bp));
    }
// 合并时，前后block的状况欠考虑????
// 重置时也欠考虑
    if(next_alloc && prev_alloc) {
        return bp;
    }else if(next_alloc && !prev_alloc) {   // 与前一个合并
        uint32_t new_set = PACK(GET_SIZE(PREV_P(bp)) + bsize, 0);
        PUT(PREV_P(bp), new_set);
        PUT(FTRP(bp+4), new_set);
        return PREV_P(bp);

    }else if(!next_alloc && prev_alloc) {   // 与后一个合并
        uint32_t new_set = PACK(GET_SIZE(NEXT_P(bp)) + bsize, 0);
        PUT(FTRP(NEXT_P(bp)+4), new_set);
        PUT(bp, new_set);
        return bp;
    }else {                                 // 与前后合并
        uint32_t new_set = PACK(GET_SIZE(PREV_P(bp)) + GET_SIZE(NEXT_P(bp)) + bsize, 0);
        PUT(PREV_P(bp), new_set);
        PUT(FTRP(NEXT_P(bp)+4), new_set);
        return PREV_P(bp);
    }
}

void *extend_heap(uint32_t size) {
    void *extended_p;
    uint32_t extended = round_up(size, 8);
    extended_p = sbrk(extended);
    if(extended_p == (void *)-1){
        fputs("Failed to extend heap\n", stderr);
        return (void *)-1;
    }
    // 为新拓展的块
    PUT(extended_p, PACK(extended, 0));
    PUT(FTRP(extended_p + 4), PACK(extended, 0));
    heap_end_p += extended;
    return coalesce(extended_p);
}


int mem_init(){
    uint32_t init_size = 128 * PAGE;
    heap_begin_p = sbrk(init_size);
    if(heap_begin_p == (void *)-1){
        fputs("Failed to extend heap\n", stderr);
        return -1;
    }
    heap_begin_p = (void *)round_up((uint64_t)heap_begin_p, 8);
    uint32_t extended_size = init_size - (uint64_t)(heap_begin_p + init_size) % 8;
    PUT(heap_begin_p, PACK(extended_size, 0));
    PUT(FTRP(heap_begin_p + 4), PACK(extended_size, 0));
    heap_end_p = heap_begin_p + extended_size;
    inited = 1;
    return 0;
}

void *find_fit(uint32_t required) {
    void *bp;
    for(bp = heap_begin_p; bp < heap_end_p; bp = NEXT_P(bp)){
        if(GET_ALLOC(bp) == 0 && required <= GET_SIZE(bp)) {
            return bp;
        }
    }
    return NULL;
}

void *mem_alloc(uint32_t size)
{
    if(!size) return NULL;
    if(!inited) mem_init();
    void *bp;
    uint32_t required = round_up(size, 8) + 8;
    bp = find_fit(required);
    if(bp != NULL) {
        place(bp, required);
        return bp + 4;
    }
    bp = extend_heap(round_up(size, PAGE) * 2);
    if(bp != (void *)-1) {
        place(bp, required);
        return bp + 4;
    }

    return NULL;
}


void mem_free(void *ap) {
    if(ap < heap_begin_p || heap_end_p <= ap) return;
    uint32_t size = GET_SIZE(ap-4);
    PUT(ap-4, PACK(size, 0));
    PUT(FTRP(ap), PACK(size, 0));
    coalesce(ap-4);
}


// malloc
// free
// calloc
// realloc

void *malloc(size_t size) {
    // fputs("malloc here\n", stderr);
    return mem_alloc(size);
}
void free(void *p) {
    // fputs("free here\n", stderr);
    mem_free(p);
}

// 分配num个元素，每个元素占nsize bytes
void *calloc(size_t num, size_t nsize) {
    size_t size;
    void *ap;
    if(!num || !nsize) return NULL;
    size = num * nsize;
    if(num != size / nsize) return NULL;
    ap = malloc(size);
    if(ap == NULL) return NULL;
    memset(ap, 0, size);
    return ap;
}

void *realloc(void *ap, size_t size) {
    void *ret;
    if(!ap || !size) return NULL;
    if(GET_SIZE(ap-4) >= size) return ap;
    ret = malloc(size);
    if(ret) {
        memcpy(ret, ap, size);
        free(ap);
    }
    return ret;
}


// int main() {

//     void *p1 = mem_alloc(8);
//     void *p2 = mem_alloc(8);
//     void *p3 = mem_alloc(8);

//     mem_free(p3);
//     mem_free(p2);
//     mem_free(p1);

//     return 0;
// }