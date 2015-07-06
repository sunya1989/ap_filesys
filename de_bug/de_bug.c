//
//  de_bug.c
//  ap_tester
//
//  Created by sunya on 15/7/2.
//  Copyright (c) 2015年 sunya. All rights reserved.
//

#include <stdio.h>
#include <debug.h>

void de_bug_init()
{
    de_bug_items = MALLOC_IPC_HASH(DE_BUG_HASH_LEN);
}

