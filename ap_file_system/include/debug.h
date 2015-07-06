//
//  debug.h
//  ap_tester
//
//  Created by sunya on 15/7/2.
//  Copyright (c) 2015年 sunya. All rights reserved.
//

#ifndef ap_tester_debug_h
#define ap_tester_debug_h
#define DE_BUG_HASH_LEN 1024
#define DEBUG_WATCH(i) __watch_item(i)
#define DEBUG_PRINT(c) __print_item(c)
#include <ap_hash.h>

struct ap_hash *de_bug_items;
struct de_bug_struct{
    struct hash_union de_bug_un;
    void (*print)(void *);
};

static inline void de_bug_item_init(struct de_bug_struct *dbi)
{
    dbi->de_bug_un.ide.ide_c = NULL;
    dbi->de_bug_un.ide.ide_i = 0;
    dbi->print = NULL;
}
void de_bug_init();
void __watch_item(struct de_bug_struct *dbi);
void __print_item(const char *ide_c);
#endif
