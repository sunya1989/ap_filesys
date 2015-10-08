/*
 *   Copyright (c) 2015, HU XUKAI
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License.
 *
 */

#ifndef ap_file_system_bag_h
#define ap_file_system_bag_h

#define BAG_HEAD_INIT(name) { &name.list, NULL, &name.list }
#define SHOW_TRASH_BAG    struct bag_head *____bag_l; \
                          do{____bag_l = MALLOC_BAG_HEAD();}while(0)
#define TRASH_BAG_PUSH(b) do{__bag_push(b, ____bag_l);}while(0)
#define TRASH_BAG_RAW_PUSH(t,f) do{\
                                struct bag *____bag = MALLOC_BAG();\
                                ____bag->trash = t;\
                                ____bag->release = f;\
                                __bag_push(____bag, ____bag_l);\
                            }while(0)

#define BAG_LOOSE(b) do{(b)->count++;}while(0)
#define B__return   do{__bag_release(____bag_l); return;}while(0)
#define B_return(r) do{__bag_release(____bag_l); return r;}while(0)


#define BAG_PUSH(b,bag_h) do{__bag_push(b, bag_h);}while(0)

#define BAG_RAW_PUSH(t,f,bag_h) do{\
                                    struct bag *____bag = MALLOC_BAG();\
                                    ____bag->trash = t;\
                                    ____bag->release = f;\
                                    __bag_push(____bag, bag_h);\
                                    }while(0)

#define BAG_POUR(bag_h)   do{__bag_pour(bag_h);}while(0)
#define BAG_EXCUTE(bag_h) do{__bag_release(bag_h);}while(0)
#define BAG_POP(bag_h) __bag_pop(bag_h)
#define BAG_RES_POP(bag_h) __bag_res_pop(bag_h)
#define BAG_EMPTY(bag_h) __bag_empty(bag_h)
#define BAG_REWIND(bag_h) __bag_rewind_pos(bag_h)

#define BAG_DEFINE_FREE(func)                                                   \
extern void BAG_##func(void *trash)

#define BAG_IMPOR_FREE(func, str)                                               \
void BAG_##func(void *trash)                                                    \
{                                                                               \
    str *t = (str *)trash;                                                      \
    func(t);                                                                    \
    return;                                                                     \
}

struct bag{
    int is_embed;
    int count;
    struct bag *next;
    void *trash;
    void (*release)(void*);
};

struct bag_head{
    struct bag **pos;
    struct bag *list;
    struct bag **list_tail;
};

extern struct bag_head global_bag;

extern void __bag_push(struct bag *bag, struct bag_head *head);
extern void __bag_release(struct bag_head *head);
extern void __bag_pour(struct bag_head *head);
extern void *__bag_pop(struct bag_head *head);
extern void *__bag_res_pop(struct bag_head *head);

static inline int __bag_empty(struct bag_head *head)
{
    return (head->list == NULL || *head->pos == NULL);
}

static inline void __bag_rewind_pos(struct bag_head *head)
{
    head->pos = &head->list;
}

extern struct bag_head *MALLOC_BAG_HEAD();
extern struct bag *MALLOC_BAG();
extern void clean_file(void *path);
#endif
