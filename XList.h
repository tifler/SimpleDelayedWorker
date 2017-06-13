#ifndef __XLIST_H__
#define __XLIST_H__

struct XList {
    struct XList *prev, *next;
};

#define offsetof(st, m) __builtin_offsetof(st, m)

#define XListEntry(ptr, type, member)   ({  \
        const typeof(((type *)0)->member) *__mptr = (ptr); \
        (type *)((char *)__mptr - offsetof(type,member));})

static inline void XListInit(struct XList *ptr)
{
    ptr->next = ptr;
    ptr->prev = ptr;
}

static inline int XListEmpty(struct XList *ptr)
{
    return ptr->next == ptr;
}

static inline void XListAddTail(struct XList *item, struct XList *head)
{
    struct XList *prev = head->prev;

    head->prev = item;
    item->next = head;
    item->prev = prev;
    prev->next = item;
}

static inline void XListInsertBefore(struct XList *item, struct XList *node)
{
    item->next = node;
    item->prev = node->prev;
    item->prev->next = item;
    node->prev = item;
}

static inline void XListDel(struct XList *ptr)
{
    struct XList *prev = ptr->prev;
    struct XList *next = ptr->next;

    next->prev = ptr->prev;
    prev->next = ptr->next;
    ptr->next = ptr;
    ptr->prev = ptr;
}

#define XListForEachEntry(pos, head, member)    \
    for (pos = XListEntry((head)->next, typeof(*pos), member);  \
            &pos->member != (head);    \
            pos = XListEntry(pos->member.next, typeof(*pos), member))

#define XListForEachEntrySafe(pos, n, head, member)          \
    for (pos = XListEntry((head)->next, typeof(*pos), member),  \
            n = XListEntry(pos->member.next, typeof(*pos), member); \
            &pos->member != (head); \
            pos = n, n = XListEntry(n->member.next, typeof(*n), member))


#endif  /*__XLIST_H__*/
