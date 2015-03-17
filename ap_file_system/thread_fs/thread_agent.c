//
//  thread_age.c
//  ap_editor
//
//  Created by x on 15/3/14.
//  Copyright (c) 2015年 sunya. All rights reserved.
//

#include <stdio.h>
#include "thread_agent.h"
static ssize_t thread_age_read(struct ger_stem_node *stem, char *buf, off_t off_set, size_t len)
{
    struct thread_age_attribute *thr_attr = container_of(stem, struct thread_age_attribute, thr_stem);
    ssize_t n_read;
    n_read = thr_attr->thr_attr_ops->attr_read(buf,stem,off_set,len);
    return n_read;
}

static ssize_t thread_age_write(struct ger_stem_node *stem, char *buf, off_t off_set, size_t len)
{
    struct thread_age_attribute *thr_attr = container_of(stem, struct thread_age_attribute, thr_stem);
    ssize_t n_write;
    n_write = thr_attr->thr_attr_ops->attr_write(buf,stem,off_set,len);
    return n_write;
}

static 