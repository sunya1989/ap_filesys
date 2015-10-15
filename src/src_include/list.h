/*
 *   Copyright (c) 2015, HU XUKAI
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License.
 *
 */
#ifndef ap_file_system_list_h
#define ap_file_system_list_h

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <envelop.h>
#define container_of(ptr, type, member) ({                      \
          const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
          (type *)( (char *)__mptr - _offsetof(type,member) );})

#define _offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

struct list_head{
	struct list_head *prev;
	struct list_head *next;
};

static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list;
    list->prev = list;
}

static inline void list_add(struct list_head *new_h, struct list_head *head)
{
    if (new_h == NULL || head == NULL) {
        ap_err("list can't be null\n");
        exit(1);
    }
    new_h->prev = head;
    new_h->next = head->next;
    head->next->prev = new_h;
    head->next = new_h;
}

static inline void list_add_tail(struct list_head *new_h, struct list_head *head)
{  
  if (new_h == NULL || head == NULL) {
        ap_err("list can't be null\n");
        exit(1);
    }
    new_h->next = head;
    new_h->prev = head->prev;
    head->prev->next = new_h;
    head->prev = new_h;
}

static inline int list_empty(struct list_head *head)
{
    return head->next == head;
}

static inline void __list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void list_del(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    entry->next = entry;
    entry->prev = entry;
}

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define list_entry(ptr, type, member) \
container_of(ptr, type, member)

#define list_first_entry(ptr, type, member) \
list_entry((ptr)->next, type, member)

#define list_next_entry(pos, member) \
list_entry((pos)->member.next, typeof(*(pos)), member)

#define list_for_each(pos, head) \
for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_middle(pos, middle, head) \
for (pos = (middle)->next; pos != (head); pos = pos->next)


#define list_for_each_use(pos1, pos2, head) \
for (pos1 = (head)->next, pos2 = (head)->next->next; pos1 != (head); pos1 = pos2, pos2 = pos1->next)

#define list_for_each_prev(pos, head) \
for (pos = (head)->prev; pos != (head); pos = pos->prev)

#define list_for_each_entry(pos, head, member)				\
for (pos = list_first_entry(head, typeof(*pos), member);	\
&pos->member != (head);					\
pos = list_next_entry(pos, member))

#define list_for_each_entry_middle(pos, head, middle, member)   \
for (pos = list_first_entry(middle, typeof(*pos), member);	\
&pos->member != (head);					\
pos = list_next_entry(pos, member))

#endif