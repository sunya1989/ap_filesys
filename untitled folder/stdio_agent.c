//
//  stdio_agent.c
//  ap_file_system
//
//  Created by sunya on 14/12/26.
//  Copyright (c) 2014年 sunya. All rights reserved.
//

#include "stdio_agent.h"
#include <fcntl.h>
#include <unistd.h>

static struct stem_file_operations std_age_file_operations={
    
};

static struct stem_inode_operations std_age_inode_operations={
    
};


ssize_t stdio_age_read(struct ger_stem *stem, char *buf, off_t off_set, size_t len)
{
    struct std_age *sa = container_of(stem, struct std_age, stem);
    ssize_t n_read;
    
    n_read = read(sa->fd, buf, len);
    return n_read;
}

ssize_t stdio_age_write(struct ger_stem *stem, char *buf, off_t off_set, size_t len)
{
    struct std_age *sa = container_of(stem, struct std_age, stem);
    ssize_t n_write;
    
    n_write = write(sa->fd, buf, len);
    return n_write;
}

off_t stdio_age_llseek(struct ger_stem *stem, off_t off_set, int origin)
{
    struct std_age *sa = container_of(stem, struct std_age, stem);
    off_t off_size;
    
    off_size = lseek(sa->fd, off_size, origin);
    return off_size;
}

int stdio_age_open(struct ger_stem *stem, unsigned long flags)
{
    struct std_age *sa = container_of(stem, struct std_age, stem);
    
    if (sa->fd != -1) {
        return sa->fd;
    }
    sa->fd = open(sa->target_file, (int)flags);
    return sa->fd;
}

int stdio_age_destory(struct ger_stem *stem)
{
    struct std_age *sa = container_of(stem, struct std_age, stem);
    STD_AGE_FREE(sa);
    return 0;
}

