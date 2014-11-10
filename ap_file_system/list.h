
#include <stdio.h>

#define container_of(ptr, type, member) ({                      \
          const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
          (type *)( (char *)__mptr - offsetof(type,member) );})

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)


struct list_head{
	struct list_head *pre;
	struct list_head *next;
};

static inline void list_add(struct list_head *_new, struct list_head *head)
{
    if (list_add == NULL || list_head == NULL) {
      perror("list can't be null");
        exit 1;
    }
    _new->pre = head;
    _new->next = head->next;
    head->next = _new;
    if (head->next != NULL)
      head->next->pre = _new;
}

static inline void list_add_tail(struct list_head *_new, struct list_head *head)
{  
  if (list_add == NULL || list_head == NULL) {
      perror("list can't be null");
        exit 1;
    }
    _new->next = head;
    _new->pre = head->pre;
    head->pre = _new;
    if (head->pre != NULL)
      head->pre->next = _new;
}