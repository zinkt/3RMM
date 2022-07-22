

/*************************************************
 * block链表头文件
 * reference linux kernel implementation
 *************************************************/

#ifndef DLIST_H_
#define DLIST_H_

#ifndef NULL
#define NULL 0
#endif

typedef struct dlist_node_s dlist_node_t;

struct dlist_node_s {
    dlist_node_t *next, *prev;
};

static inline void dlist_init(dlist_node_t *head, dlist_node_t *tail) {
    if (head == NULL || tail == NULL) {
        return; 
    }

    head->prev = NULL;
    head->next = tail;
    tail->prev = head;
    tail->next = NULL;
}

static inline int dlist_empty(dlist_node_t *head, dlist_node_t *tail) {
    return (head->next == tail && tail->prev == head);
}

static inline void dlist_remove(dlist_node_t *elem) {
    elem->prev->next = elem->next;
    elem->next->prev = elem->prev;
}

static inline void dlist_add(dlist_node_t* head, dlist_node_t *elem) {
    dlist_node_t *next = head->next;

    head->next = elem;
    elem->prev = head;
    elem->next = next;
    next->prev = elem;
}

#endif 

