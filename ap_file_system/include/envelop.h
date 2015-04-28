//
//  envelop.h
//  ap_editor
//
//  Created by sunya on 15/4/26.
//  Copyright (c) 2015年 sunya. All rights reserved.
//

#ifndef ap_file_system_envelop_h
#define ap_file_system_envelop_h
static inline void *Malloc(size_t size)
{
    char *buf = malloc(size);
    if (buf == NULL) {
        perror("malloc failed\n");
        exit(1);
    }
    return buf;
}

#endif
