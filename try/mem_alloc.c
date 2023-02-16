#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>


#define PAGE 4096

// 假设了sbrk分配的空间全为0

int mem_init();
void *mem_alloc(uint32_t size);
void mem_free(void * p);

void * heap_begin_p;
// void * heap_end_p; 不需要

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

void *extend_heap(uint32_t size) {
    void *extended_p;
    uint32_t extended = round_up(size, 8);
    if(extended_p = sbrk(extended) == -1){
        printf("Failed to extend heap\n");
        return -1;
    }
    // 为新拓展的块
    PUT(extended_p, PACK(extended, 0));
    PUT(FTRP(extended_p + 4), PACK(extended, 0));

    return coalesce(extended_p);
}


int mem_init(){
    if(heap_begin_p = sbrk(PAGE) == -1){
        printf("Failed to extend heap\n");
        return -1;
    }
    heap_begin_p = (void *)round_up((uint64_t)heap_begin_p, 8);
    uint32_t extended_size = PAGE - (uint64_t)(heap_begin_p + PAGE) % 8;
    PUT(heap_begin_p, PACK(extended_size, 0));
    PUT(FTRP(heap_begin_p + 4), PACK(extended_size, 0));
    return 0;
}

void *find_fit(uint32_t required) {
    void *bp;
    for(bp = heap_begin_p; GET_SIZE(bp) > 0; bp = NEXT_P(bp)){
        if(GET_ALLOC(bp) == 0 && required <= GET_SIZE(bp)) {
            return bp;
        }
    }
    return NULL;
}

void *mem_alloc(uint32_t size)
{
    if(!size) return NULL;

    void *bp;
    uint32_t required = round_up(size, 8) + 8;
    if(bp = find_fit(required) != NULL) {
        place(bp, required);
        return bp + 4;
    }
    
    if(bp = extend_heap(PAGE) != -1) {
        place(bp, required);
        return bp + 4;
    }

    return NULL;
}

// 尝试合并bp所指向的block的前后两个block，并返回合并后的空闲block首地址
void *coalesce(void *bp) {
    uint32_t bsize = GET_SIZE(bp);
    uint32_t next_alloc = GET_ALLOC(NEXT_P(bp));
    uint32_t prev_alloc = GET_ALLOC(PREV_P(bp));

    if(next_alloc && prev_alloc) {
        return bp;
    }else if(next_alloc && !prev_alloc) {   // 与前一个合并
        PUT(PREV_P(bp), PACK(GET_SIZE(PREV_P(bp)) + bsize, 0));
        PUT(FTRP(PREV_P(bp)+4), PACK(GET_SIZE(PREV_P(bp)) + bsize, 0));
        return PREV_P(bp);

    }else if(!next_alloc && prev_alloc) {   // 与后一个合并
        PUT(NEXT_P(bp), PACK(GET_SIZE(NEXT_P(bp)) + bsize, 0));
        PUT(FTRP(NEXT_P(bp)+4), PACK(GET_SIZE(NEXT_P(bp)) + bsize, 0));
        return bp;
    }else {                                 // 与前后合并
        PUT(PREV_P(bp), PACK(GET_SIZE(PREV_P(bp)) + GET_SIZE(PREV_P(bp)) + bsize, 0));
        PUT(FTRP(PREV_P(bp)+4), PACK(GET_SIZE(PREV_P(bp)) + GET_SIZE(PREV_P(bp)) + bsize, 0));
        return PREV_P(bp);
    }
}


void mem_free(void *p) {
    uint32_t size = GET_SIZE(p-4);
    PUT(p-4, PACK(size, 0));
    PUT(FTRP(p), PACK(size, 0));
    coalesce(p);
}

int main(){}