/**
 * @file myMalloc_2.c
 * @author Mike Li (mike_lwj@foxmail.com)
 * @brief A simple implementation of malloc
 * Model:
 * ||header|payload||header|payload||...||header|payload||end_ptr
 * header = payload_size | flag
 *
 * @version 0.2.1
 * @date 2022-04-02
 *
 * @copyright Copyright (c) 2022
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <malloc.h>
#include <time.h>

////基本操作、变量宏定义////
// 默认初始化堆大小1KB(128 * 8)
#define MAX_HEAP (1 * (1 << 10))
//最小块大小16B(尽可能减少外部碎片的产生)
#define MIN_BLOCK (16)

//对齐标准
#define ALIGN sizeof(size_t)
// 按对齐读取
#define GET(p) (*(size_t *)(p))
// 按对齐写
#define PUT(p, val) (*((size_t *)(p)) = val)

// 空载标识
#define EMPTY (0)
// 负载标识
#define ALLOC (1)
// 包装header
#define PACK(payload, flag) ((payload) | (flag))

// 读payload大小
#define GET_PAYLOAD(p) (GET(p) & ~0x7)
// 读flag大小
#define GET_FLAG(p) (GET(p) & 0x1)

// 从有效载荷首地址读header
#define H_PTR(bp) (((char *)(bp)) - ALIGN)
// 从header读下一个header
#define NEXT_H_PTR(p) (((char *)(p)) + GET_PAYLOAD(p) + ALIGN)
// 从header得payload地址
#define PAYLOAD_PTR(p) (((char *)(p)) + ALIGN)

////函数,变量声明////
// 使用size_t跨平台移植性(x32:4Byte,x64:8Byte)
// 堆底header
static size_t *heap_beg = NULL;
// 堆顶+1
static size_t *heap_end = NULL;
// 当前搜索起始块的header
static size_t *heap_cur = NULL;
// 有效容量
static size_t empty_size = 0;
// 堆初始化标识
static bool init_flag = true;

/**
 * @brief malloc入口
 *
 * @param size      请求分配大小
 * @return void*    有效载荷首地址;失败时,返回NULL
 */
void *my_malloc(size_t size);

/**
 * @brief 释放堆空间,向后合并空块
 *
 * @param bp     待释放块的有效载荷首地址
 */
void my_free(void *bp);

/**
 * @brief 初始化堆内存
 *
 * @param aligned_size      对齐后的请求大小
 * @return void*            返回首块的有效载荷地址
 */
void *heap_init(size_t aligned_size);

/**
 * @brief 搜索有效空块
 *
 * @param aligned_size      对齐后搜索目标的大小
 * @return void*            返回空块有效载荷首地址;失败时,返回NULL
 */
void *search_empty_bp(size_t aligned_size);

/**
 * @brief 分配空块(划分新块),重置heap_cur
 *
 * @param bp                目标空块有效载荷首地址
 * @param aligned_size      对齐后所需有效载荷大小
 */
void alloc_block(void *bp, size_t aligned_size);

/**
 * @brief 归并堆中的空块
 *
 */
void merge();

/**
 * @brief 使用sbrk()拓展heap,重置heap_cur
 *
 * @param aligned_size      对齐后当前需求有效块的大小
 * @return void*            返回所需的有效载荷首地址
 */
void *extend(size_t aligned_size);

////实现////

void *my_malloc(size_t size)
{
    // 请求堆大小 修正 使内存对齐
    size += ALIGN - size % ALIGN;
    // 初始化目标块指针
    size_t *empty_bp = NULL;

    // 初始化堆空间,控制指针
    if (init_flag == true)
    {
        // 重置初始化标识
        init_flag = false;
        empty_bp = heap_init(size);
        if (empty_bp == NULL)
        {
            // 初始化失败
            // printf("Error[1]: heap_init() fail");
            return NULL;
        }
        else
        {
            // 初始化成功,且首块符合
            return empty_bp;
        }
    }
    // 初始化成功

    // 初始化归并标识
    static bool merge_flag = true;

    // 现有适合的空有效块 可能存在
    while (size < empty_size)
    {
        empty_bp = search_empty_bp(size);
        if (empty_bp != NULL)
        {
            // 有 有效块
            alloc_block(empty_bp, size);
            merge_flag = true;
            return empty_bp;
        }
        else if (merge_flag == true)
        {
            // 无 有效块;进行空块归并
            merge();
            // 重置归并标识
            merge_flag = false;
        }
        else
        {
            // 归并后依旧 无 有效块
            // 不存在合适的有效块
            break;
        }
    }
    // 现有适合的空有效块 不存在
    return extend(size);
}

void my_free(void *bp)
{
    // 临时存储连续空块大小
    size_t temp_empty_size = GET_PAYLOAD(H_PTR(bp));
    //更新有效容量
    empty_size += temp_empty_size;
    size_t *test = (size_t *)NEXT_H_PTR(H_PTR(bp));
    // 搜索后继空块
    for (size_t *cur = (size_t *)NEXT_H_PTR(H_PTR(bp)); GET_FLAG(cur) != ALLOC && cur != heap_end; cur = (size_t *)NEXT_H_PTR(cur))
    {
        temp_empty_size += GET_PAYLOAD(cur) + ALIGN;
        //追加header释放
        empty_size += ALIGN;
    }
    // 更新bp指向块的header(即,释放时归并后继连续空块)
    PUT(H_PTR(bp), PACK(temp_empty_size, EMPTY));

    // heap_cur
}

void *heap_init(size_t aligned_size)
{
    if (aligned_size >= MAX_HEAP)
    {
        // 默认堆大小不足
        // 初始化两倍aligned_size的堆,并获得堆底
        heap_beg = (size_t *)sbrk(2 * aligned_size + 2 * ALIGN);
        // 初始化空余有效空间大小
        empty_size = aligned_size;
    }
    else
    {
        // 默认堆大小充足
        // 初始化默认大小的堆,并获得堆底
        heap_beg = (size_t *)sbrk(MAX_HEAP + 2 * ALIGN);
        // 初始化空余有效空间大小
        empty_size = MAX_HEAP - aligned_size;
    }
    // 获得堆顶+1
    heap_end = (size_t *)sbrk(0);
    // 设置首块的header
    PUT(heap_beg, PACK(aligned_size, ALLOC));
    // 置位cur指针(下一次堆分配搜索起点)
    // size_t a = GET_PAYLOAD(heap_beg);
    // size_t b = ALIGN;
    // heap_cur = ((char *)(heap_beg)) + b + a;
    heap_cur = (size_t *)NEXT_H_PTR(heap_beg);
    // 设置下一块的header
    PUT(heap_cur, PACK(empty_size, EMPTY));
    return PAYLOAD_PTR(heap_beg);
}

void *search_empty_bp(size_t aligned_size)
{
    // 初始化搜索指针
    size_t *cur = NULL;
    for (cur = heap_cur;
         (GET_FLAG(cur) == ALLOC || GET_PAYLOAD(cur) < aligned_size) && cur != heap_end;
         cur = (size_t *)NEXT_H_PTR(cur))
        ;
    if (cur != heap_end)
    {
        // heap_cur及其后 有 合适块
        return PAYLOAD_PTR(cur);
    }
    // heap_cur及其后 无 合适块
    // 搜索heap_cur前的块
    for (cur = heap_beg;
         (GET_FLAG(cur) == ALLOC || GET_PAYLOAD(cur) < aligned_size) && cur != heap_cur;
         cur = (size_t *)NEXT_H_PTR(cur))
        ;
    if (cur != heap_cur)
    {
        // heap_cur前 有 合适块
        return PAYLOAD_PTR(cur);
    }
    // 当前堆空间 无 合适有效块
    return NULL;
}

void alloc_block(void *bp, size_t aligned_size)
{
    size_t *header_bp = (size_t *)H_PTR(bp);
    size_t payload_size = (size_t)GET_PAYLOAD(header_bp);
    if (payload_size >= aligned_size + 2 * ALIGN + MIN_BLOCK)
    {
        //可以划分新块(外部碎片)
        //更新header
        PUT(header_bp, PACK(aligned_size, ALLOC));
        //创建新header(原块容量-分配大小-新header)
        PUT(NEXT_H_PTR(header_bp), PACK(payload_size - aligned_size - ALIGN, EMPTY));
        //更新有效容量(原有效-分配大小-新header)
        empty_size -= aligned_size + ALIGN;
    }
    else
    {
        //无法划分新块,直接分配(内部碎片)
        PUT(header_bp, PACK(payload_size, ALLOC));
    }
    //更新下一次搜索起点
    heap_cur = (size_t *)NEXT_H_PTR(header_bp);
}

void merge()
{
    //归并起始点指针
    size_t *cur = heap_beg;
    //归并结束点指针
    size_t *test = heap_beg;
    //记录连续空块大小
    size_t temp_empty_size = 0;
    //双指针遍历归并
    for (; cur < heap_end; cur = (size_t *)NEXT_H_PTR(cur))
    {
        //找到起始空块
        if (GET_FLAG(cur) == EMPTY)
        {
            temp_empty_size += GET_PAYLOAD(cur);
            //搜索连续空块结束位置
            for (test = (size_t *)NEXT_H_PTR(cur); GET_FLAG(test) == EMPTY && test != heap_end; test = (size_t *)NEXT_H_PTR(test))
            {
                temp_empty_size += GET_PAYLOAD(test) + ALIGN;
            }
            if (temp_empty_size != GET_PAYLOAD(cur))
            {
                //存在连续空块,合并空块,更新header
                PUT(cur, PACK(temp_empty_size, EMPTY));
            }
            temp_empty_size = 0;
            //从test开始重新搜索空块起点
            cur = test;
        }
    } //遍历至heap_end,结束,归并完成
    //置下次搜索起点为堆顶+1,迫使直接冲头搜索可能空块
    heap_cur = heap_end;
}

void *extend(size_t aligned_size)
{
    //扩容需求大小+默认堆初始化大小
    sbrk(aligned_size + 2 * ALIGN + MAX_HEAP);
    //更新为header(end -> header)
    PUT(heap_end, PACK(aligned_size, ALLOC));
    //更新下次搜索起点
    heap_cur = (size_t *)NEXT_H_PTR(heap_end);
    //创建新的header
    PUT(heap_cur, PACK(MAX_HEAP, EMPTY));
    //临时存储目标块header
    size_t *empty_block = heap_end;
    //更新堆顶+1
    heap_end = (size_t *)sbrk(0);
    //更新有效容量
    empty_size += MAX_HEAP;
    return PAYLOAD_PTR(empty_block);
}

/**
 * @brief 测试用例
 * 
 */
void __test_case();

int main(void)
{
    // Test Case(分配、扩容、搜索、归并、释放)

    __test_case();

    exit(0);
}

void __test_case()
{
    printf("////Test Case Running////\n");
    int *segment[10];
    for (int i = 0; i < 10; segment[i] = NULL, ++i)
        ;
    srand((unsigned int)time(NULL));
    unsigned int step = 0;
    int oper = 0;
    int index = 0;

    clock_t start;
    clock_t end;

    while (step < 100)
    {
        oper = rand() % 10;  // 0~9
        index = rand() % 10; // 0~9
        switch (oper)
        {
        case 9:
            if (segment[index] != NULL)
            {
                start = clock();
                my_free(segment[index]);
                end = clock();
                printf("step[%d]: segment[%d] do[%d] cost: %f\n", step, index, oper, (double)(end - start) / CLOCKS_PER_SEC);
                segment[index] = NULL;
            }
            else
            {
                continue;
            }
            break;

        default:
            if (segment[index] != NULL)
            {
                continue;
            }
            oper = rand() % 200 + 500;
            start = clock();
            segment[index] = my_malloc(oper);
            end = clock();
            size_t *test = (size_t *)GET_PAYLOAD(H_PTR(segment[index]));
            printf("step[%d]: segment[%d] do[%d] cost: %f\n", step, index, oper, (double)(end - start) / CLOCKS_PER_SEC);
            break;
        }
        for (size_t *cur = heap_beg; cur <= heap_end; cur = (size_t *)NEXT_H_PTR(cur))
        {
            printf("%p: %ld | %ld; \n", cur, GET_PAYLOAD(cur), GET_FLAG(cur));
        }
        ++step;
    }
}
