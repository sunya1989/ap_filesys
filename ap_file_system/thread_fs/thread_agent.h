//
//  thread_age.h
//  ap_editor
//
//  Created by x on 15/3/14.
//  Copyright (c) 2015年 sunya. All rights reserved.
//

#ifndef ap_editor_thread_age_h
#define ap_editor_thread_age_h

#include <ger_fs.h>
#include <pthread.h>

struct thread_attr_operations{
    void *(*attr_read)(void *, struct ger_stem_node*);
    int (*attr_write)(void *, struct ger_stem_node*);
};

struct thread_attribute_age{
    struct ger_stem_node thread_stem;
    struct thread_attr_operations *thr_attr_ops;
    void *x_object;
};
struct thread_age_dir{
    struct ger_stem_node thread_stem;
    
    
};

#endif
