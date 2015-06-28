//
//  bag.c
//  ap_tester
//
//  Created by sunya on 15/6/25.
//  Copyright (c) 2015年 sunya. All rights reserved.
//

#include <stdio.h>
#include <bag.h>
#include <envelop.h>

struct bag{
    struct bag *next;
    void *trash;
    void (*release)(void*);
};

struct bag_head{
    struct bag *list;
    struct bag **list_tail;
};

struct bag_head *MALLOC_BAG_HEAD()
{
    struct bag_head *bh;
    bh = Mallocz(sizeof(*bh));
    bh->list = NULL;
    bh->list_tail = &bh->list;
    return bh;
}

struct bag *MALlOC_BAG()
{
    struct bag *bg = Mallocz(sizeof(*bg));
    bg->release = NULL;
    bg->trash = NULL;
    return bg;
}

void __bag_push(struct bag *bag, struct bag_head *head)
{
    *head->list_tail = bag;
    head->list_tail = &bag->next;
}

void __bag_release(struct bag_head *head)
{
    struct bag *bg;
    
    while (head->list != NULL) {
        bg = head->list;
        bg->release(bg->trash);
        head->list = bg->next;
        free(bg);
    }
}







