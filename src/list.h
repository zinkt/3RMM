/*
 * 链表头文件
 * 参考<linux/list.h>
 */

#ifndef WL_LIST_H
#define WL_LIST_H

#ifndef NULL
#define NULL 0
#endif

struct list_head {
    struct list_head *prev, *next;
};

typedef struct list_head list_head;

// 初始化链表头
static inline void INIT_LIST_HEAD(struct list_head *list){
	list->next = list;
	list->prev = list;
}

// 在prev和next间插入新节点_new
static inline void __list_add(struct list_head *_new, struct list_head *prev, struct list_head *next) {
    _new->next = next;
    next->prev = _new;
    prev->next = _new;
    _new->prev = prev;
}

static inline void list_add(struct list_head *_new, struct list_head *head) {
    __list_add(_new, head, head->next);
}

static inline void list_add_tail(struct list_head *_new, struct list_head *head) {
    __list_add(_new, head->prev, head);
}

// 删除：处理前后node的指针
static inline void __list_del(struct list_head *prev, struct list_head *next) {
    prev->next = next;
    next->prev = prev;
}

// 将entry从链表中删除
static inline void list_del(struct list_head *entry) {
    __list_del(entry->prev, entry->next);
    //entry->next = entry->prev = NULL;
}

// 检查链表是否为空
static inline int list_empty(const struct list_head *list) {
    return (list->next == list) && (list->prev == list);
}

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );     \
})

// 由节点指针ptr，获取所属结构体type指针。其中member是list_head在type中的成员名
#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_prev(pos, head) \
    for (pos = (head)->prev; pos != (head); pos = pos->prev)


#define list_for_each_entry(pos, head, member)  \
    for (pos = list_entry((head)->next, typeof(*pos), member);  \
        &(pos->member) != (head);                               \
        pos = list_entry(pos->member.next, typeof(*pos), member))

static inline void __list_splice(const struct list_head *list,
				 struct list_head *prev,
				 struct list_head *next)
{
	struct list_head *first = list->next;
	struct list_head *last = list->prev;

	first->prev = prev;
	prev->next = first;

	last->next = next;
	next->prev = last;
}

/**
 * list_splice - join two lists, this is designed for stacks
 * @list: the new list to add.
 * @head: the place to add it in the first list.
 */
static inline void list_splice(const struct list_head *list,
				struct list_head *head)
{
	if (!list_empty(list))
		__list_splice(list, head, head->next);
}

/**
 * list_splice_tail - join two lists, each list being a queue
 * @list: the new list to add.
 * @head: the place to add it in the first list.
 */
static inline void list_splice_tail(struct list_head *list,
				struct list_head *head)
{
	if (!list_empty(list))
		__list_splice(list, head->prev, head);
}

#endif 
