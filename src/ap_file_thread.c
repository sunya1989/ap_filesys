/*
 *   Copyright (c) 2015, HU XUKAI
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License.
 *
 */
#include <stdlib.h>
#include <ap_pthread.h>
#include <ap_file.h>
#include <bag.h>

pthread_key_t file_thread_key;
pthread_once_t thread_once = PTHREAD_ONCE_INIT;
static void thread_file_destory(void *my_data)
{
    free(my_data);
}

static void ap_at_exit()
{
    BAG_EXCUTE(&global_bag);
}

static void thread_init()
{
    int err = pthread_key_create(&file_thread_key, thread_file_destory);
    if (err) {
        ap_err("ap_thread_init failed\n");
        exit(1);
    }
    
    atexit(ap_at_exit);
}

int ap_file_thread_init()
{
    struct ap_file_pthread *file_pthr;
    
    pthread_once(&thread_once, thread_init);
    file_pthr = Malloc_z(sizeof(*file_pthr));
    
    AP_FILE_THREAD_INIT(file_pthr);
    
    int set = pthread_setspecific(file_thread_key, file_pthr);
    if (set) {
        ap_err("ap_thread set failes\n");
        exit(1);
    }
    return 0;
}