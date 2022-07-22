#include "wlmalloc.h"
#include <pthread.h>
#include <unistd.h>
#include <string.h>
/* 全局元数据 */
pthread_once_t init_once = PTHREAD_ONCE_INIT;
thread_state_t global_state = UNINIT;
pthread_key_t destructor;   //key一旦被创建，所有线程都可以访问它，但各线程可根据自己的需要往key中填入不同的值，这就相当于提供了一个同名而不同值的全局变量，一键多值。
global_pool_t pool;
thread_metadata_t tcmeta;    //该结构体储存了thread_cache们的元数据所在位置

/* mappings */
CACHE_ALIGN int cls2size[128] = {16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120, 128, 136, 144, 152, 160, 168, 176, 184, 192, 200, 208, 216, 224, 232, 240, 248, 256, 384, 512, 768, 1024, 1536, 2048, 3072, 4096, 6144, 8192, 12288, 16384, 24576, 32768, 49152, 65536, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,};
char sizemap[256] = {0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,};
char sizemap2[128] = {0, 34, 35, 36, 37, 37, 38, 38, 39, 39, 39, 39, 40, 40, 40, 40, 41, 41, 41, 41, 41, 41, 41, 41, 42, 42, 42, 42, 42, 42, 42, 42, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46,};

/* threads */
THREAD_LOCAL thread_state_t thread_state = UNINIT;
THREAD_LOCAL thread_cache_t *local_thread_cache = NULL;

void *wl_malloc(size_t sz){
    void *ret = NULL;
    check_init();
    
    sz += (sz == 0);
    uint8_t size_cls = size2cls(sz);
    if(likely(size_cls<DEFUALT_CLASS_NUM)){
        ret = small_alloc(size_cls);
    }else if(size_cls == LARGE_CLASS){
        ret = large_alloc(sz);
    }else{
        fprintf(stderr, "Fatal: wl_malloc failed\n");
        exit(-1);
    }
    return ret;
}
void wl_free(void *ptr){
    if(ptr ==NULL){
        return;
    }
    span_t *span = GET_HEADER(ptr);
    thread_cache_t *span_owner = span->owner;

    if((uint64_t)span_owner != LARGE_OWNER){    //小内存
        span_free_blk(span, ptr);
    }else{
        syscall_free(span->free_blk, span->blk_size);
    }
}


 static void *large_alloc(size_t sz){   //???
    size_t total = sz + sizeof(span_t); //??? + SPAN_SIZE
    void *raw = syscall_alloc(NULL, total);
    void *ret = (void *)ROUNDUP((uint64_t)raw, SPAN_SIZE);  //???
    span_t *span = (span_t *)ret;
    span->owner = (thread_cache_t *)LARGE_OWNER;    //只是作为区别于其他任何tc
    span->free_blk = raw;
    span->blk_size = total;
    return ret + sizeof(span_t);
 }

static void *small_alloc(uint8_t size_cls){
    span_t *span = local_thread_cache->using[size_cls];
    //如果无可用blk，先回收
    if(span->free_total_cnt == 0){
        remote_blk_recycle(local_thread_cache);
    }
    //如果回收后，这个span还是无可用blk。或者是VACANT的（第一次分配）
    if(span->free_total_cnt == 0 || span->cls == VACANT_CLASS){
        span->state = FULL;
        swap_span_in(local_thread_cache, size_cls);
    }
    //处理当前可用的span
    span = local_thread_cache->using[size_cls];
    return get_blk(span);
}

static void *get_blk(span_t *span){
    void *ret = NULL;
    if(span->free_raw_cnt > 0){ //还有连续的未分配空间
        ret = span->free_blk;
        span->free_blk += span->blk_size;
        span->free_raw_cnt--;
    }else{  //从空闲链表获取(已保证free_total_cnt>0)
        ret = (void *)span->blks_tail.prev;
        dlist_remove((dlist_node_t *)ret);  //将其从空闲链表中删去
    }
    span->free_total_cnt--; //无论如何，分配完成后，free_total_cnt--;
    return ret;
}


static void swap_span_in(thread_cache_t *tc, uint8_t size_cls){
    span_t *span;
    do
    {
        if(!list_empty(&(local_thread_cache->suspend[size_cls]))) {
            span = list_entry(local_thread_cache->suspend[size_cls].next, span_t, list);
            list_del(&span->list);
            break;
        }
        if(!list_empty(&local_thread_cache->free_list)) {
            span = list_entry(local_thread_cache->free_list.next, span_t, list);
            list_del(&span->list);
            span->owner = tc;
            span_init(span, size_cls);
            break;
        }
        //向pool获取
        span = acquire_spans();
        span->owner = tc;
        span_init(span, size_cls);
    } while (0);
    //using换成新的span
    local_thread_cache->using[size_cls] = span;
    span->state = IN_USE;
}

static span_t *acquire_spans(){
    pthread_mutex_lock(&pool.lock);

    span_t *span = pool.free_start;
    uint32_t acquire = local_thread_cache->acquire_cnt;
    if(pool.free_start + (acquire+1)*SPAN_SIZE <= pool.end){
        void *ptr = pool.free_start + SPAN_SIZE;
        pool.free_start += (acquire+1)*SPAN_SIZE;
        pthread_mutex_unlock(&pool.lock);
        // ???注意ptr
        for (size_t i = 0; i < acquire; i++){
            list_add(&((span_t *)ptr)->list, &local_thread_cache->free_list);
            ptr += SPAN_SIZE;
        }
    }else if(!list_empty(&pool.free_list)){  //在freelist中找
        //这种情况，返回一个即可
        list_add(pool.free_list.next, &local_thread_cache->free_list);
        list_del(pool.free_list.next);
        pthread_mutex_unlock(&pool.lock);

    }else{  //向操作系统申请
        void *raw = syscall_alloc(pool.end, ALLOC_UNIT);
        if (raw != pool.end) {
            fprintf(stderr, "Fatal: syscall_alloc() failed\n");
            exit(-1);
        }
        pool.end += ALLOC_UNIT;
        void *ptr = pool.free_start + SPAN_SIZE;
        pool.free_start += (acquire+1)*SPAN_SIZE;
        pthread_mutex_unlock(&pool.lock);
        for (size_t i = 0; i < acquire; i++){
            list_add(&((span_t *)ptr)->list, &local_thread_cache->free_list);
        }
    }
    return span;
}

static void span_init(span_t *span, uint8_t size_cls){
    span->cls = size_cls;
    span->blk_size = cls2size[size_cls];
    span->free_total_cnt = span->free_raw_cnt = (SPAN_DATA_SIZE) / span->blk_size;
    span->free_blk = (void *)span + sizeof(span_t);
    //初始化空闲blk链表
    dlist_init(&span->blks_head, &span->blks_tail);
    
}

static void remote_blk_recycle(thread_cache_t *tc){
    pthread_mutex_lock(&local_thread_cache->lock);
    list_head *blk;
    list_for_each(blk, &tc->remote_blk_list){
        list_del(blk);
        span_t *span = GET_HEADER(blk);
        span_free_blk(span, blk);
    }
    pthread_mutex_unlock(&local_thread_cache->lock);
}

static void span_free_blk(span_t *span, void *blk){
    thread_cache_t *owner = span->owner;
    if(owner == local_thread_cache){
        //这里直接利用blk的空间，储存了prev和next两个指针，用于将blk链接在链表中。因此blk至少为16byte
        dlist_add(&span->blks_head, (dlist_node_t *)blk);
        span->free_total_cnt++;
        
        uint8_t size_cls = span->cls;
        //修改span状态
        switch (span->state)
        {
        case IN_USE:    //正在被使用时，无需操作（cnt已在上面+1）
            break;
        case FULL:      //span已满
            span->state = SUSPEND;
            list_add_tail(&span->list, &local_thread_cache->suspend[size_cls]);
            break;
        case SUSPEND:   //被挂起，且所有blk都被free时
            if(unlikely(span->free_total_cnt == SPAN_DATA_SIZE/(span->blk_size))){
                list_del(&span->list);
                list_add_tail(&span->list, &local_thread_cache->free_list);
            }
            break;
        default:
            fprintf(stderr, "Fatal: span state error\n");
            exit(-1);
            break;
        }
    }else{  //其他线程的blk
        pthread_mutex_lock(&owner->lock);
        list_add_tail((list_head *)blk, &owner->remote_blk_list);
        pthread_mutex_unlock(&owner->lock);
    }
}

static uint8_t size2cls(size_t sz){
    uint8_t cls;
    if(likely(sz <= 1024)) {
        cls = sizemap[(sz-1)>>3];
    }else if(sz <= 65536) {
        cls = sizemap2[(sz-1)>>9];
    }else{
        cls = LARGE_CLASS;
    }
    return cls;
}


static void check_init(){
    if (unlikely(thread_state != INITED)){
        if (unlikely(global_state != INITED)){
            pthread_once(&init_once, global_init);
        }
        //线程第一次malloc，则创建一个thread_cache，赋值给tc
        thread_cache_init();
    }
}

static void thread_cache_init(){
    //保证线程退出时调用thread_exit()
    pthread_setspecific(destructor, (void *)1);

    pthread_mutex_lock(&tcmeta.lock);
    local_thread_cache = tcmeta.free_start;
    do
    {
        //剩余空间充足
        if(tcmeta.free_start + TC_SIZE <= tcmeta.end){
            tcmeta.free_start += TC_SIZE;
            break;
        }
        //空闲链表中有被释放的空闲meta块
        if(!list_empty(&tcmeta.free_list)){
            local_thread_cache = list_entry(tcmeta.free_list.next, thread_cache_t, list);
            list_del(&local_thread_cache->list);
            break;
        }
        //再次申请
        thread_metadata_expand();
        tcmeta.free_start += TC_SIZE;
    } while (0);
    pthread_mutex_unlock(&tcmeta.lock);

    for (int i = 0; i < DEFUALT_CLASS_NUM; i++){
        //vacant用于占位
        local_thread_cache->using[i] = &(local_thread_cache->vacant);
        INIT_LIST_HEAD(&(local_thread_cache->suspend[i]));
    }
    INIT_LIST_HEAD(&local_thread_cache->free_list);
    INIT_LIST_HEAD(&local_thread_cache->remote_blk_list);
    
    local_thread_cache->vacant.owner = local_thread_cache;
    local_thread_cache->vacant.blk_size = 0;
    local_thread_cache->vacant.cls = VACANT_CLASS;
    local_thread_cache->vacant.free_raw_cnt = 0;
    local_thread_cache->vacant.free_total_cnt = 1;
    local_thread_cache->vacant.free_blk = (void *)0;
    dlist_init(&local_thread_cache->vacant.blks_head, &local_thread_cache->vacant.blks_tail);

    //由SLOW_STARTS多个span开始，每次向pool申请时，申请2*acquire_cnt个
    local_thread_cache->acquire_cnt = SLOW_STARTS;

    if(pthread_mutex_init(&local_thread_cache->lock, NULL)<0){
        fprintf(stderr, "Fatal: thread_cache_init() failed\n");
        exit(-1);
    }

    thread_state = INITED;
}

static void thread_metadata_expand(){
    void *raw = syscall_alloc(tcmeta.end, TC_ALLOC_UNIT);
    if(raw != tcmeta.end){
        fprintf(stderr, "Fatal: thread_metadata_expand() failed\n");
        exit(-1);
    }
    tcmeta.end += TC_ALLOC_UNIT;
}

static void global_init() {
    //保证线程退出时调用thread_exit()
    pthread_key_create(&destructor, thread_exit);
    global_pool_init();
    global_state = INITED;
}

static void thread_exit() {
    list_add_tail(&local_thread_cache->list, &tcmeta.free_list);
    //在local_thread_cache中的span呢???
    
}

static void global_pool_init(){
    if(pthread_mutex_init(&pool.lock, NULL)<0 || pthread_mutex_init(&tcmeta.lock, NULL)<0){
        fprintf(stderr, "Fatal: pthread_mutex_init() failed\n");
        exit(-1);
    }
    //第一次初始化时预先申请的内存
    void *raw = syscall_alloc(RAW_POOL_START, ALLOC_UNIT);
    if((uint64_t)raw < 0){
        fprintf(stderr, "Fatal: syscall_alloc() failed\n");
        exit(-1);
    }
    
    //初始化pool
    pool.start = (void *)(((uint64_t)raw + SPAN_SIZE - 1)/SPAN_SIZE*SPAN_SIZE);
    pool.end = raw + ALLOC_UNIT;
    pool.free_start = pool.start;
    INIT_LIST_HEAD(&pool.free_list);
    //分配空间给thread_cache元数据
    raw = syscall_alloc(NULL, TC_ALLOC_UNIT);
    if(raw < (int)0){
        fprintf(stderr, "Fatal: syscall_alloc() failed\n");
        exit(-1);    
    }
    tcmeta.start = (void *)(((uint64_t)raw + TC_SIZE - 1)/TC_SIZE*TC_SIZE);
    tcmeta.end = raw + TC_ALLOC_UNIT;
    tcmeta.free_start = tcmeta.start;
    INIT_LIST_HEAD(&tcmeta.free_list);
}

static void *syscall_alloc(void *pos, size_t sz){
    //sbrk

    //mmap
    return mmap(pos, sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    //addr: 指定映射被放置的虚拟地址，如果将addr指定为NULL，那么内核会为映射分配一个合适的地址。
    //如果addr为一个非NULL值，则内核在选择地址映射时会将该参数值作为一个提示信息来处理。
    //不管采用何种方式，内核会选择一个不与任何既有映射冲突的地址。
    //在处理过程中， 内核会将指定的地址舍入到最近的一个分页边界处。
}

static void syscall_free(void *pos, size_t sz){
    //sbrk

    //mmap
    munmap(pos,sz);
}



