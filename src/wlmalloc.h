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
#include "list.h"

/* 基础定义 */
#define likely(x)           __builtin_expect(!!(x),1)
#define unlikely(x)         __builtin_expect(!!(x),0)
// #define CACHE_LINE_SIZE             (64)
// #define CACHE_ALIGN __attribute__   ((aligned (CACHE_LINE_SIZE)))
#define THREAD_LOCAL __attribute__  ((tls_model ("initial-exec"))) __thread

#define POOL_OWNER              (0xbbbb)
#define LARGE_OWNER             (0xaaaa)
#define LARGE_CLASS             (31)
#define DEFAULT_CLASS_NUM       (29)

/* config */
// #define SLOW_STARTS             (2)

#define PAGE                4096
#define SPAN_DATA_SIZE      (4*PAGE)
#define SPAN_SIZE           (SPAN_DATA_SIZE+sizeof(span_t))
#define ALLOC_UNIT          (1024*1024*128)
// 对x取到n的整
#define ROUNDUP(x,n)        (((x)+(n)-1)/(n)*(n))
// ???对齐的问题
#define GET_HEADER(ptr)     ((span_t *)((size_t)ptr - (size_t)(ptr) % SPAN_SIZE))

typedef struct span_s span_t;
typedef struct tcache_s tcache_t;
typedef struct gpool_s gpool_t;

typedef enum {
    IN_USE,
    SUSPEND,
    FREE,
} span_state_t;


struct span_s
{
    // NULL: 表示处于gpool中
    tcache_t *owner;
    span_state_t state;
    char cls;
    //blk_cnt = SPANDATA_SIZE/blk_size
    uint32_t blk_size;

    void *free_blk; //当前可直接分配的blk
    uint32_t free_cnt;    //所有空闲的blk数（包括在空闲blk_list中的） 

    list_head blk_list; //空闲blk链表头

    list_head node;   //管理span自己

};

struct tcache_s
{
    // 其他线程free时加锁，只对suspend[]和free_list加锁
    pthread_mutex_t lock;

    //当前所有大小类正在使用的span的指针
    span_t *using[DEFAULT_CLASS_NUM];

    //所有大小类已使用过的span的链表头
    list_head suspend[DEFAULT_CLASS_NUM];

    //空闲span的链表头
    list_head free_list;
};

//提供span和大内存请求
struct gpool_s
{
    pthread_mutex_t lock;
    void *start;
    void *end;
    void *free_start;
    // 完全释放的空闲span
    list_head free_list;
    // 未完全释放的span
    list_head suspend[DEFAULT_CLASS_NUM];
};


/*********************************
 *********************************/

// 内存分配接口
void *wl_malloc(size_t sz);

// 内存释放接口
void wl_free(void *ptr);

/*********************************
 * 三模冗余的读（或者说check）
 * 三模冗余的写
 *********************************/
void *tri_mod_read(void *ptr);
void *tri_mod_write(void *ptr, void *source, size_t size);


/*********************************
 * 大于65536byte的请求，调用mmap
 * 将信息装填入LARGE_CLASS的span中
 *********************************/
 static void *large_alloc(size_t sz);

/*********************************
 * 从该span中获取一个blk
 *********************************/
 static void *get_blk(span_t *span);

/*********************************
 * 从pool获取一个可用span(已初始化，但需要调整owner和state)
 *********************************/
 static span_t *acquire_span(char size_cls);

/*********************************
 * 找到可用的span作为IN_USE
 *********************************/
 static void replace_available_span(tcache_t *tc, char size_cls);

/*********************************
 * 初始化span(不包括owner和state)
 *********************************/
 static void span_init(span_t *span, char size_cls);

/*********************************
 * 按span回收blk：回收span中以及被free的blk
 *********************************/
 static void span_free_blk(span_t *span, void *blk);

/*********************************
 * 回收由其他线程free的，即free_blk_list中的blk块
 *********************************/
 static void remote_blk_recycle(tcache_t *tc);

/*********************************
 * 分配小内存，<16384byte
 *********************************/
 static void *small_alloc(char size_cls);

/*********************************
 * 储存thread_cache的空间不足时，申请扩充
 *********************************/
 static void thread_metadata_expand();

/*********************************
 * 初始化thread_cache
 *********************************/
 static void thread_cache_init();

/*********************************
 * 系统调用获取一块内存空间
 *********************************/
 static void *syscall_alloc(size_t sz);

/*********************************
 * 释放由系统调用获取的内存空间
 *********************************/
 static int syscall_free(void *pos, size_t sz);

/*********************************
 * 线程退出时被调用
 * 完成归还内存、等操作
 *********************************/
 static void thread_exit();

/*********************************
 * 只执行一次，用于初始化全局元数据
 *********************************/
 static void global_init();

/*********************************
 * 检查进程以及线程是否首次malloc
 * 进程首次需要初始化
 * 线程首次需要分配thread_cache
 *********************************/
 static void check_init();

/*********************************
size映射到class
 *********************************/
 static char size2cls(size_t sz);
