
#include <stdio.h>
#include <stdlib.h>
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

static inline void list_add(struct list_head *_new, struct list_head *head)
{
    if (_new == NULL || head == NULL) {
      perror("list can't be null");
        exit(1);
    }
    _new->prev = head;
    _new->next = head->next;
    head->next = _new;
    head->next->prev = _new;
}

static inline void list_add_tail(struct list_head *_new, struct list_head *head)
{  
  if (_new == NULL || head == NULL) {
      perror("list can't be null");
        exit(1);
    }
    _new->next = head;
    _new->prev = head->prev;
    head->prev = _new;
    head->prev->next = _new;
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

#define list_for_each_prev(pos, head) \
for (pos = (head)->prev; pos != (head); pos = pos->prev)

#define list_for_each_entry(pos, head, member)				\
for (pos = list_first_entry(head, typeof(*pos), member);	\
&pos->member != (head);					\
pos = list_next_entry(pos, member))