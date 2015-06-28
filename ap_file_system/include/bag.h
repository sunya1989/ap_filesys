//
//  bag.h
//  ap_tester
//
//  Created by sunya on 15/6/25.
//  Copyright (c) 2015年 sunya. All rights reserved.
//

#ifndef ap_file_system_bag_h
#define ap_file_system_bag_h

#define SHOW_BAG    struct bag_head *____bag_l; \
                    do{____bag_l = MALLOC_BAG_HEAD();}while(0)
#define BAG_PUSH(b) do{__bag_push(b, ____bag_l);}while(0)
#define BAG_RAW_PUSH(t,f) do{\
                                struct bag *____bag = MALLOC_BAG();\
                                bag->trash = t;\
                                bag->release = f\
                                ____bag_push(____bag, ____bag_l)\
                            }while(0)
#define B__return   do{__bag_release(____bag_l); return;}while(0)
#define B_return(r) do{__bag_release(____bag_l); return r;}while(0)

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
    struct bag *next;
    void *trash;
    void (*release)(void*);
};

struct bag_head;

extern void __bag_push(struct bag *bag, struct bag_head *head);
extern void __bag_release(struct bag_head *head);

extern struct bag_head *MALLOC_BAG_HEAD();
extern struct bag *MALLOC_BAG();
#endif
