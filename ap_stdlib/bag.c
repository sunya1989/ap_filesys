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

struct bag_head *MALLOC_BAG_HEAD()
{
    struct bag_head *bh;
    bh = Mallocz(sizeof(*bh));
    bh->list = NULL;
    bh->pos = bh->list_tail = &bh->list;
    return bh;
}

struct bag *MALLOC_BAG()
{
    struct bag *bg = Mallocz(sizeof(*bg));
    bg->release = NULL;
    bg->trash = NULL;
    bg->is_embed = 0;
    return bg;
}

void __bag_push(struct bag *bag, struct bag_head *head)
{
    *head->list_tail = bag;
    head->list_tail = &bag->next;
}

void *__bag_pop(struct bag_head *head)
{
    if (head->list == NULL) {
        return NULL;
    }
    void *t;
    struct bag *bg = head->list;
    head->list = bg->next;
    t = bg->trash;
    if (!bg->is_embed) {
        free(bg);
    }
    
    return t;
}

void *__bag_res_pop(struct bag_head *head)
{
    if (*head->pos == NULL) {
        return NULL;
    }
    void *t;
    struct bag *bg = *head->pos;
    t = bg->trash;
    head->pos = &bg->next;
    return t;
}

void __bag_release(struct bag_head *head)
{
    struct bag *bg;
    struct bag *next;
    int embed;
    
    while (head->list != NULL) {
        bg = head->list;
        next = bg->next;
        embed = bg->is_embed;
        bg->release(bg->trash);
        head->list = next;
        if (!embed) {
            free(bg);
        }
    }
}

void __bag_pour(struct bag_head *head)
{
    struct bag *bg;
    struct bag *next;
    int embed;
    
    while (head->list != NULL) {
        bg = head->list;
        next = bg->next;
        embed = bg->is_embed;
        head->list = next;
        if (!embed) {
            free(bg);
        }
    }
}







