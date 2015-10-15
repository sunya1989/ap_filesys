/*
 *   Copyright (c) 2015, HU XUKAI
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License.
 *
 */
#ifndef __ap_file_system__stdio_agent__
#define __ap_file_system__stdio_agent__

#include <stdio.h>
#include <unistd.h>
#include <ap_fsys/ger_fs.h>
#include <ap_fsys/envelop.h>

enum file_state{
    g_fileno = 0,
    g_stream,
};

struct std_age{
    struct ger_stem_node stem;
    char *target_file;
};

struct std_age_dir{
    const char *target_dir;
    struct ger_stem_node stem;
};

extern void STD_AGE_INIT(struct std_age *age, char *tarf);
extern void STD_AGE_DIR_INIT(struct std_age_dir *age_dir, const char *tard);

static inline struct std_age *MALLOC_STD_AGE(char *tarf)
{
    struct std_age *sa;
    sa = malloc(sizeof(*sa));
    if (sa == NULL) {
        ap_err("malloc std_age failed\n");
        exit(1);
    }
    
    STD_AGE_INIT(sa,tarf);
    return sa;
}

static inline struct std_age_dir *MALLOC_STD_AGE_DIR(const char *tard)
{
    struct std_age_dir *sa_dir;
    sa_dir = malloc(sizeof(*sa_dir));
    if (sa_dir == NULL) {
        ap_err("malloc std_age_dir failed\n");
        exit(1);
    }
    
    STD_AGE_DIR_INIT(sa_dir, tard);
    return sa_dir;
}

static inline void STD_AGE_FREE(struct std_age *sa)
{
    COUNTER_FREE(&sa->stem.stem_inuse);
    pthread_mutex_destroy(&sa->stem.ch_lock);
    free(sa);
}

static inline void STD_AGE_DIR_FREE(struct std_age_dir *sa_dir)
{
    COUNTER_FREE(&sa_dir->stem.stem_inuse);
    pthread_mutex_destroy(&sa_dir->stem.ch_lock);
    free(sa_dir);
}

#endif /* defined(__ap_file_system__stdio_agent__) */

