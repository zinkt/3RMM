#include "wlmalloc.h"
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

// 控制进程只执行一次
pthread_once_t init_once = PTHREAD_ONCE_INIT;
// 用于线程退出处理函数
pthread_key_t destructor;
// 全局内存池
gpool_t pool;

/* mappings 生成示例在sizeclass.c中 */
// 29个类
int cls2size[32] = {16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048, 3072, 4096, 6144, 8192, 12288, 16384};
// sizemap8[(size-1) >> 3] 等于 class
char sizemap8[64] = {0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 16, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18};
// sizemap512[(size-1) >> 8] 等于 class
char sizemap256[64] = {0, 18, 19, 20, 21, 21, 22, 22, 23, 23, 23, 23, 24, 24, 24, 24, 25, 25, 25, 25, 25, 25, 25, 25, 26, 26, 26, 26, 26, 26, 26, 26, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28};


/* threads */
THREAD_LOCAL char thread_state = 0;
THREAD_LOCAL tcache_t local_tcache;

void *wl_malloc(size_t sz){
    fprintf(stderr, "my malloc...\n");
    void *ret = NULL;
    check_init();
    if(!sz) return ret;

    char size_cls = size2cls(sz);
    if(likely(size_cls<DEFAULT_CLASS_NUM)){
        ret = small_alloc(size_cls);
    }else if(size_cls == LARGE_CLASS){
        ret = large_alloc(sz);
    }else{
        fprintf(stderr, "Fatal: Requested space is too large\n");
        exit(-1);
    }
    return ret;
}

void wl_free(void *ptr){
    fprintf(stderr, "my free...\n");
    if(ptr == NULL){
        return;
    }
    span_t *span;
    if(ptr < pool.start || pool.end < ptr) {
        span = ptr - sizeof(span_t);
        if(syscall_free((void *)span, span->blk_size) != 0) {
            fprintf(stderr, "munmap failed\n");
            exit(-1);
        }
        return;
    }
    span = GET_HEADER(ptr);
    tcache_t *span_owner = span->owner;
    span_free_blk(span, ptr);
}

 static void *large_alloc(size_t sz){
    size_t total = sz + sizeof(span_t);
    void *raw = syscall_alloc(sz);
    if(raw == (void *)-1) {
        fprintf(stderr, "mmap failed\n");
    }
    span_t *span = (span_t *)raw;
    // ???? 大内存分配时，span首地址计算问题？
    span->owner = (tcache_t *)LARGE_OWNER;    //只是作为区别于其他任何tc
    span->blk_size = sz;
    return raw + sizeof(span_t);
 }

static void *small_alloc(char size_cls){
    span_t *span = local_tcache.using[size_cls];
    // 如果该大小类第一次分配
    if(span == NULL) {
        replace_available_span(&local_tcache, size_cls);
        span = local_tcache.using[size_cls];
    }
    //如果为已用完
    else if(span->free_cnt == 0){
        // 将已满的span链入suspend
        span->state = SUSPEND;
        list_add_tail(&span->node ,&local_tcache.suspend[size_cls]);
        // 获取新span
        replace_available_span(&local_tcache, size_cls);
        span = local_tcache.using[size_cls];
    }
    //处理当前可用的span
    return get_blk(span);
}

static void replace_available_span(tcache_t *tc, char size_cls){
    span_t *span;
    do{
        // 从suspend[]链表中获取
        if(!list_empty(&(local_tcache.suspend[size_cls]))) {
            list_for_each_entry(span, &(local_tcache.suspend[size_cls]), node) {
                if(span->free_cnt > 0) {
                    list_del(&span->node);
                    break;
                }
            }
        }
        // 从free_list中获取
        if(!list_empty(&local_tcache.free_list)) {
            span = list_entry(local_tcache.free_list.next, span_t, node);
            list_del(&span->node);
            span_init(span, size_cls);
            break;
        }
        //向pool获取
        span = acquire_span(size_cls);
    } while (0);
    //using换成新的span
    local_tcache.using[size_cls] = span;
    span->owner = tc;
    span->state = IN_USE;
}
static void *get_blk(span_t *span){
    void *ret = NULL;
    // 使用blk_list中的
    if(!list_empty(&span->blk_list)) {
        ret = span->blk_list.next;
        list_del(span->blk_list.next);
        span->free_cnt--;
        return ret;
    }
    // 使用连续的未分配空间
    ret = span->free_blk;
    span->free_blk += span->blk_size;
    span->free_cnt--;
    return ret;
}

static span_t *acquire_span(char size_cls){
    pthread_mutex_lock(&pool.lock);
    span_t *span;
    // 尝试从suspend中获取
    if(!list_empty(&(pool.suspend[size_cls]))) {
        span = list_entry(pool.suspend[size_cls].next, span_t, node);
        list_del(pool.suspend[size_cls].next);
        pthread_mutex_unlock(&pool.lock);
        return span;
    }
    // 尝试从free_list中获取
    if(!list_empty(&pool.free_list)) {
        span = list_entry(pool.free_list.next, span_t, node);
        list_del(pool.free_list.next);
        pthread_mutex_unlock(&pool.lock);
        span_init(span, size_cls);
        return span;
    }
    // 尝试从大内存中切出一块
    // ???考虑大内存处理
    if(pool.free_start + SPAN_SIZE <= pool.end){
        span = pool.free_start;
        pool.free_start += SPAN_SIZE;
        pthread_mutex_unlock(&pool.lock);
        span_init(span, size_cls);
        return span;
    }
    // 向操作系统申请
    void *raw = syscall_alloc(ALLOC_UNIT);
    if (raw != pool.end) {
        fprintf(stderr, "Fatal: syscall_alloc() failed\n");
        exit(-1);
    }
    pool.end += ALLOC_UNIT;
    span = pool.free_start;
    pool.free_start += SPAN_SIZE;
    pthread_mutex_unlock(&pool.lock);
    span_init(span, size_cls);
    return span;
}

static void span_init(span_t *span, char size_cls){
    span->cls = size_cls;
    span->blk_size = cls2size[size_cls];
    span->free_cnt = (SPAN_DATA_SIZE) / span->blk_size;
    span->free_blk = (void *)span + sizeof(span_t);
    //初始化空闲blk_list
    INIT_LIST_HEAD(&span->blk_list);
}

static void span_free_blk(span_t *span, void *blk){
    tcache_t *owner = span->owner;
    if(owner == &local_tcache){
        span->free_cnt++;
        if(unlikely(span->state == SUSPEND && span->free_cnt == SPAN_DATA_SIZE/(span->blk_size))){
            list_del(&span->node);
            list_add_tail(&span->node, &local_tcache.free_list);
            // 重置blk_list链表
            INIT_LIST_HEAD(&span->blk_list);
            span->state = FREE;
        }
        // 这里直接利用blk的空间，储存了prev和next两个指针，用于将blk链接在链表中。因此blk至少为16byte
        list_add((list_head *)blk ,&span->blk_list);
    }
    // 属于已回收回pool的span
    else if(owner == (tcache_t *)POOL_OWNER){
        pthread_mutex_lock(&pool.lock);
        span->free_cnt++;
        if(unlikely(span->state == SUSPEND && span->free_cnt == SPAN_DATA_SIZE/(span->blk_size))){
            list_del(&span->node);
            list_add_tail(&span->node, &pool.free_list);
            // 重置blk_list链表
            INIT_LIST_HEAD(&span->blk_list);
            span->state = FREE;
        }
        list_add((list_head *)blk ,&span->blk_list);
        pthread_mutex_unlock(&local_tcache.lock);
    }
    // 其他线程的blk
    else {
        pthread_mutex_lock(&owner->lock);
        span->free_cnt++;
        if(unlikely(span->state == SUSPEND && span->free_cnt == SPAN_DATA_SIZE/(span->blk_size))){
            list_del(&span->node);
            list_add_tail(&span->node, &local_tcache.free_list);
            // 重置blk_list链表
            INIT_LIST_HEAD(&span->blk_list);
            span->state = FREE;
        }
        list_add((list_head *)blk ,&span->blk_list);
        pthread_mutex_unlock(&owner->lock);
        
    }
}



static void check_init(){
    if (unlikely(!thread_state)){
        // 进程初始化，保证只执行一次
        pthread_once(&init_once, global_init);
        //线程第一次malloc，则创建一个thread_cache，赋值给tc
        thread_cache_init();
    }
}

static void thread_cache_init(){
    //保证线程退出时调用thread_exit()
    pthread_setspecific(destructor, (void *)1);
    // 初始化lock
    if(pthread_mutex_init(&local_tcache.lock, NULL)<0){
        fprintf(stderr, "Fatal: thread_cache_init() failed\n");
        exit(-1);
    }
    // 初始化using[]和suspend[]
    for (int i = 0; i < DEFAULT_CLASS_NUM; i++){
        local_tcache.using[i] = NULL;
        INIT_LIST_HEAD(&(local_tcache.suspend[i]));
    }
    // 初始化free_list
    INIT_LIST_HEAD(&local_tcache.free_list);
    
    // //由SLOW_STARTS多个span开始，每次向pool申请时，申请2*acquire_cnt个
    // local_tcache->acquire_cnt = SLOW_STARTS;

    thread_state = 1;
}

static void global_init() {
    //保证线程退出时调用thread_exit()
    pthread_key_create(&destructor, thread_exit);

    if(pthread_mutex_init(&pool.lock, NULL) < 0){
        fprintf(stderr, "Fatal: pthread_mutex_init() failed\n");
        exit(-1);
    }
    //第一次初始化时预先申请的内存
    void *raw = syscall_alloc(ALLOC_UNIT);
    if(raw == (void *)-1){
        fprintf(stderr, "Fatal: syscall_alloc() failed\n");
        exit(-1);
    }
    
    //初始化pool
    // ???待处理 对齐
    pool.start = (void *)(((size_t)raw + SPAN_SIZE - 1)/SPAN_SIZE*SPAN_SIZE);
    pool.end = raw + ALLOC_UNIT;
    pool.free_start = pool.start;
    INIT_LIST_HEAD(&pool.free_list);
    for(int i = 0; i < DEFAULT_CLASS_NUM; i++){
        INIT_LIST_HEAD(&(pool.suspend[i]));
    }
}

static void thread_exit() {
    // inuse的span链入suspend
    for(int i = 0; i < DEFAULT_CLASS_NUM; i++) {
        if(local_tcache.using[i] != NULL) {
            list_add_tail(&local_tcache.using[i]->node, &local_tcache.suspend[i]);
        }
    }
    span_t *span;
    pthread_mutex_lock(&pool.lock);
    // 所有suspend的链入pool的suspend中
    for (int i = 0; i < DEFAULT_CLASS_NUM; i++){
        list_for_each_entry(span, &(local_tcache.suspend[i]), node){
            span->owner = (tcache_t *)POOL_OWNER;
        }
        if(!list_empty(&local_tcache.suspend[i])) {
            list_splice_tail(&local_tcache.suspend[i], &pool.suspend[i]);
        }
    }
    // 处理free_list中的
    list_splice_tail(&local_tcache.free_list, &pool.free_list);

    pthread_mutex_unlock(&pool.lock);
}

static void *syscall_alloc(size_t sz){
    // fprintf(stderr, "mmap\n");
    // return malloc(sz);
    
    //mmap
    return mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    //addr: 指定映射被放置的虚拟地址，如果将addr指定为NULL，那么内核会为映射分配一个合适的地址。
    //如果addr为一个非NULL值，则内核在选择地址映射时会将该参数值作为一个提示信息来处理。
    //不管采用何种方式，内核会选择一个不与任何既有映射冲突的地址。
    //在处理过程中， 内核会将指定的地址舍入到最近的一个分页边界处。
}

static int syscall_free(void *pos, size_t sz){
    // fprintf(stderr, "munmap\n");
    // free(pos);
    //mmap
    return munmap(pos, sz);
}

static char size2cls(size_t sz){
    char cls;
    if(likely(sz <= 512)) {
        cls = sizemap8[(sz-1)>>3];
    }else if(sz <= 16384) {
        cls = sizemap256[(sz-1)>>8];
    }else{
        cls = LARGE_CLASS;
    }
    return cls;
}

void *tri_mod_alloc(size_t size)
{
    return wl_malloc(3*size);
}

void tri_mod_free(void *ptr)
{
    wl_free(ptr);
}

void *tri_mod_read(void *ptr, size_t size)
{
    // 分别得到三个块的指针
    void *ptrs[3] = {ptr, ptr + size, ptr + size + size};
    // 初始化三次对比的结果
    int compare[3] = {0, 0, 0};
    // 三次对比
    for (int i = 0; i < 3; i++) {
        compare[i] = memcmp(ptrs[i], ptrs[(i + 1) % 3], size);
    }
    // 检查三个块是否相同，相同则返回ptr
    if (compare[0] == 0 && compare[1] == 0) {
        return ptr;
    }
    // 检查是否出错，即三次比较都不同
    if (compare[0] != 0 && compare[1] != 0 && compare[2] != 0) {
        return NULL;
    }
    // 恢复发生翻转的块
    if (compare[0] == 0) {
        memcpy(ptrs[2], ptrs[0], size);
    } else if (compare[1] == 0) {
        memcpy(ptrs[0], ptrs[1], size);
    } else {
        memcpy(ptrs[1], ptrs[0], size);
    }
    return ptr;
}

void *tri_mod_write(void *ptr, void *source, size_t size){
    span_t* s = GET_HEADER(ptr);   // 根据传入的地址来获得此地址所在chunk的首地址
    if(!s || s->blk_size < size * 3) return (void *)-1;
    void *ptr2 = ptr + size;
    void *ptr3 = ptr2 + size;
    // 将值写入第一块，第二块和第三块对应位置
    memcpy(ptr, source, size); 
    memcpy(ptr2, source, size);
    memcpy(ptr3, source, size);
}



void *malloc(size_t size) {
    return wl_malloc(size);
}

void free(void *p) {
    wl_free(p);
}

void *calloc(size_t num, size_t nsize) {
    // fprintf(stderr, "my calloc...\n");
    size_t size;
    void *ret;
    if(!num || ! nsize) return NULL;
    size = num * nsize;
    if(size / num != nsize) return NULL;
    ret = malloc(size);
    if(ret == NULL) return NULL;
    memset(ret, 0, size);
    return ret;
}

void *realloc(void *p, size_t size) {
    // fprintf(stderr, "my realloc...\n");
    void *ret;
    if(!p || !size) return NULL;
    span_t *span;
    if(p < pool.start || pool.end < p) {
        span = p - sizeof(span_t);
        if(span->blk_size >= size) return p;
        ret = malloc(size);
        memcpy(ret, p, span->blk_size);
        if(syscall_free((void *)span, span->blk_size) != 0) {
            fprintf(stderr, "munmap failed\n");
            exit(-1);
        }
        return ret;
    }
    span = GET_HEADER(p);
    if(span->blk_size >= size) return p;
    ret = malloc(size);
    if(ret) {
        memcpy(ret, p, span->blk_size);
        free(p);
    }
    return ret;
}
