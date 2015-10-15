/*
 *   Copyright (c) 2015, HU XUKAI
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License.
 *
 */
#include <stdio.h>
#include <bag.h>
#include <unistd.h>
#include <envelop.h>

struct bag_head global_bag = BAG_HEAD_INIT(global_bag);
struct bag_head *MALLOC_BAG_HEAD()
{
    struct bag_head *bh;
    bh = Malloc_z(sizeof(*bh));
    bh->list = NULL;
    bh->pos = bh->list_tail = &bh->list;
    return bh;
}

struct bag *MALLOC_BAG()
{
    struct bag *bg = Malloc_z(sizeof(*bg));
    bg->release = NULL;
    bg->trash = NULL;
    bg->is_embed = 0;
    bg->count = 0;
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
    if (bg->count)
        bg->count--;
    
    t = bg->trash;
    if (!bg->is_embed)
        free(bg);
    
    return t;
}

void *__bag_res_pop(struct bag_head *head)
{
    if (*head->pos == NULL)
        return NULL;
    
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
        head->list = next;
        if (bg->count) {
            bg->count--;
            continue;
        }
        embed = bg->is_embed;
        bg->release(bg->trash);
        if (!embed)
            free(bg);
        
    }
    head->list_tail = &head->list;
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
        if (bg->count)
            bg->count--;
        if (!embed)
            free(bg);
    }
    head->list_tail = &head->list;
}

void clean_file(void *path)
{
    unlink(path);
}

