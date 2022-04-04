/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "zinkt",
    /* First member's full name */
    "zink t",
    /* First member's email address */
    "zinkt@foxmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// 基础常量以及宏
#define WSIZE       4
#define DSIZE       8
#define CHUNKSIZE   (1<<12)

#define MAX(x, y)   ((x) > (y) ? (x) : (y))

// 将size和已分配比特打包进一个字中
#define PACK(size, alloc)   ((size) | (alloc))
// ???????????????这感觉没有变成一个字啊，size的最低位还跟alloc运算了。。
// 解释：（猜测）size需要大于等于8，读出来时同样把低三位屏蔽掉了，所以不影响


// 在地址p上以 字 大小来读写
#define GET(p)      (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

// 从地址p上读出块大小和已分配位，p处于header或footer
#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

// 对于给定的块指针bp，计算出其header和footer的地址
// (块指针bp指向第一个有效载荷字节)
#define HDRP(bp)    ((char *)(bp) - WSIZE)
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
// ?????????????????为什么要减去DSIZE：因为size是指整个块的字节数，不是有效载荷大小

// 对于给定的块指针bp，计算出后一个或者前一个块的地址
// 注意两个的区别，减去WSIZE是找到本块的header，而减去DSIZE是找到前一个块的header
#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

static char *heap_listp;    // 总是指向序言块的下一个块

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if(prev_alloc && next_alloc){
        return bp;
    }

    else if(prev_alloc && !next_alloc){
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        // 这里需要先将header中的size修改，因为FTRP依赖于size
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if(!prev_alloc && next_alloc){ // prev未分配,next已分配
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else{
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + 
            GET_SIZE(FTRP(NEXT_BLKP(bp)));
        // ????????为什么这儿一个要用HDRP，一个用FTRP，不是都是副本，一样的吗
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}



// 将堆扩展 words 个字，并且创建初始的空闲块
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    // 分配奇数个字大小的空间，目的在于对齐
    size = (words % 2) ? (words + 1) * WSIZE : (words * WSIZE);
    if((long)(bp = mem_sbrk(size)) == (void *)-1)
        return NULL;
    // ???????????这里bp是位于什么位置？
    // 解释：位于上次sbrk申请的那段内存的结语块后面

    // 完成空闲块header/footer和结语块的初始化
    PUT(HDRP(bp), PACK(size, 0));           // 将上次sbrk申请的那段内存的结语块变成了新的空闲块头部
    PUT(FTRP(bp), PACK(size, 0));   
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   // 将这个字(新申请的块后面？)变为新的结语块

    // 如果前一个块是空闲块，则合并
    return coalesce(bp);
}

// 寻找空闲块：首次适配
static void *first_fit(size_t asize)
{
    for(void *ptr = heap_listp; GET_SIZE(HDRP(ptr)) > 0; ptr = NEXT_BLKP(ptr)){
        if(GET_ALLOC(HDRP(ptr)) == 0 && GET_SIZE(HDRP(ptr)) >= asize){
            return ptr;
        }
    }
    return NULL;
}

static void *find_fit(size_t asize)
{
    return first_fit(asize);
}

// 将大块切割出asize字节
static void place(void *bp, size_t asize)
{
    size_t bsize = GET_SIZE(HDRP(bp));  // 待切割的大小
    if(bsize - asize < 2 * DSIZE){
        PUT(HDRP(bp), PACK(bsize, 1));
        PUT(FTRP(bp), PACK(bsize, 1));
        return;
    }

    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    bp = NEXT_BLKP(bp);
    PUT(HDRP(bp), PACK(bsize - asize, 0));
    PUT(FTRP(bp), PACK(bsize - asize, 0));
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    // 申请四个字
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0);                             // "第一个字是一个双字边界对齐的不使用的填充字"
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));    // 序言 header
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));    // 序言 footer，DSIZE代表整个序言块（header+footer）占8字节
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));        // 结语块
    heap_listp += (2*WSIZE);

    if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;       // 调整后的size
    size_t extendsize;  // Amount to extend heap if not fit
    char *bp;

    if(size == 0)
        return NULL;
    if(size <= DSIZE)
        asize = 2*DSIZE;    // 要求最小块大小是16byte
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    // >8byte时，加上开销字节(两个WSIZE)，然后向上舍入到最接近的8的整数倍

    // 在空闲表中找到一个合适的空闲块
    if((bp = find_fit(asize)) != NULL){
        place(bp, asize);
        return bp;
    }
    // 没找到合适的，就扩充后，再。。
    extendsize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














