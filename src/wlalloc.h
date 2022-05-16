#ifndef WLALLOC_H_
#define WLALLOC_H_

#include <unistd.h>
#include <pthread.h>
#include <stdio.h>  
#include <stdlib.h>
// mmap
#include <sys/mman.h>
//stdint.h是C标准函数库中的头文件，定义了具有特定位宽的整型，以及对应的宏
#include <stdint.h>
//sys/types.h是基本系统数据类型的头文件，含有size_t，time_t，pid_t等类型
#include <sys/types.h>
#include "dlist.h"
#include "list.h"

/* 基础定义 */
#define likely(x)           __builtin_expect(!!(x),1)
#define unlikely(x)         __builtin_expect(!!(x),0)
#define CACHE_LINE_SIZE             (64)
#define CACHE_ALIGN __attribute__   ((aligned (CACHE_LINE_SIZE)))
#define THREAD_LOCAL __attribute__  ((tls_model ("initial-exec"))) __thread

#define LARGE_OWNER             (0x7c00)
#define LARGE_CLASS             (49)
#define VACANT_CLASS            (50)
#define DEFUALT_CLASS_NUM       (48)      //47?

/* config */
#define SLOW_STARTS             (2)

#define PAGE                4096
#define SPAN_DATA_SIZE      (64*PAGE)
#define SPAN_SIZE           (SPAN_DATA_SIZE+sizeof(span_t))
#define RAW_POOL_START      ((void *)((0x600000000000/SPAN_SIZE+1)*SPAN_SIZE))
#define ALLOC_UNIT          (1024*1024*64)
#define TC_SIZE             (sizeof(thread_cache_t))
#define TC_ALLOC_UNIT       (ROUNDUP((128*TC_SIZE),PAGE))
// 对x取到n的整
#define ROUNDUP(x,n)        ((x+n-1)/n*n)
#define GET_HEADER(ptr)     ((span_t *)((uint64_t)ptr - (uint64_t)(ptr) % SPAN_SIZE))

typedef struct span_s span_t;
typedef struct thread_cache_s thread_cache_t;
typedef struct thread_metadata_s thread_metadata_t;
typedef struct global_pool_s global_pool_t;


typedef enum {
    UNINIT,
    INITED
} thread_state_t;

typedef enum {
    IN_USE,
    SUSPEND,
    FULL,

} span_state_t;


struct span_s
{
    thread_cache_t *owner;
    uint8_t cls;
    uint32_t blk_size;
    //blk_cnt = SPANDATA_SIZE/blk_size
    span_state_t state;

    void *free_blk; //当前可直接分配的blk
    uint32_t free_raw_cnt;  //连续的blk数（即完全没被用过的blk数）
    uint32_t free_total_cnt;    //所有空闲的blk数（包括在dlist中的） 

    dlist_node_t blks_head, blks_tail;

    list_head list;   //管理span自己

    uint8_t spared_cnt;
};

struct thread_cache_s
{
    //被其它线程free的时候使用，基本无影响
    pthread_mutex_t lock;

    //当前正在使用的span
    span_t *using[DEFUALT_CLASS_NUM];    

    //已使用过的span   
    list_head suspend[DEFUALT_CLASS_NUM];

    //可用span的链表头
    list_head free_list;   

    //非本线程free的block
    list_head remote_blk_list;

    //让tc被串起来(thread_metadata中)
    list_head list;
    
    //申请span的次数，用于"慢启动"
    //由SLOW_STARTS多个span开始，每次向pool申请时，
    //额外申请2*acquire_cnt个span连接到free_list
    uint32_t acquire_cnt;

    //占位空span
    span_t vacant;
};


struct thread_metadata_s
{
    pthread_mutex_t lock;
    //申请空间，用于储存各线程thread_cache元数据
    void *start;
    void *end;
    void *free_start;
    //各线程返回的thread_cache内存块
    list_head free_list;
};

//提供span和大内存请求
struct global_pool_s
{
    pthread_mutex_t lock;
    void *start;
    void *end;
    void *free_start;
    //各线程返回的span
    list_head free_list;
    uint32_t free_list_cnt;
};




/*********************************
 *********************************/

// 内存分配接口
void *wl_malloc(size_t sz);

// 内存释放接口
void wl_free(void *ptr);

/*********************************
 * 
 *********************************/
void *tri_mod_read(void *ptr);

/*********************************
 * 大于65536byte的请求，调用mmap
 * 将信息装填入LARGE_CLASS的span中
 *********************************/
inline static void *large_alloc(size_t sz);

/*********************************
 * 从该span中获取一个blk
 *********************************/
inline static void *get_blk(span_t *span);

/*********************************
 * 返回一个span
 * 并向pool多申请一些span，插入到free_list中
 *********************************/
inline static span_t *acquire_spans();

/*********************************
 * 初始化span
 *********************************/
inline static void span_init(span_t *span, uint8_t size_cls);

/*********************************
 * 找到可用的span作为IN_USE，将原本size_cls的span放入suspend链表中
 *********************************/
inline static void swap_span_in(thread_cache_t *tc, uint8_t size_cls);

/*********************************
 * 按span回收blk：回收span中以及被free的blk
 *********************************/
inline static void span_free_blk(span_t *span, void *blk);

/*********************************
 * 回收由其他线程free的，即free_blk_list中的blk块
 *********************************/
inline static void remote_blk_recycle(thread_cache_t *tc);

/*********************************
 * 分配小内存，<65536byte
 *********************************/
inline static void *small_alloc(uint8_t size_cls);

/*********************************
 * 储存thread_cache的空间不足时，申请扩充
 *********************************/
inline static void thread_metadata_expand();

/*********************************
 * 初始化thread_cache
 *********************************/
inline static void thread_cache_init();

/*********************************
 * 系统调用获取一块内存空间
 *********************************/
inline static void *syscall_alloc(void *pos, size_t sz);

/*********************************
 * 释放由系统调用获取的内存空间
 *********************************/
inline static void syscall_free(void *pos, size_t sz);

/*********************************
 * 初始化global_pool，只执行一次
 *********************************/
inline static void global_pool_init();

/*********************************
 * 线程退出时被调用
 * 完成归还内存、等操作
 *********************************/
inline static void thread_exit();

/*********************************
 * 只执行一次，用于初始化全局元数据
 *********************************/
inline static void global_init();

/*********************************
 * 检查进程以及线程是否首次malloc
 * 进程首次需要初始化
 * 线程首次需要分配thread_cache
 *********************************/
inline static void check_init();


#endif
