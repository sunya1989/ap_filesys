//
//  envelop.h
//  ap_editor
//
//  Created by sunya on 15/4/26.
//  Copyright (c) 2015年 sunya. All rights reserved.
//

#ifndef ap_file_system_envelop_h
#define ap_file_system_envelop_h
#include <sys/msg.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
static inline void *Mallocx(size_t size)
{
    char *buf = malloc(size);
    if (buf == NULL) {
        perror("malloc failed\n");
        exit(1);
    }
    return buf;
}

static inline void *Mallocz(size_t size)
{
    char *buf = calloc(1, size);
    if (buf == NULL) {
        perror("malloc failed\n");
        exit(1);
    }
    return buf;
}

static inline int Msgget(key_t key, int flag)
{
    errno = 0;
    int msgget_s = msgget(key, flag);
    if (msgget_s == -1) {
        perror("msgget faild\n");
        exit(1);
    }
    return msgget_s;
}
#endif
