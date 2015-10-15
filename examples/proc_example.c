/*
 *   Copyright (c) 2015, HU XUKAI
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License.
 *
 */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <ap_fsys/ap_types.h>
#include <ap_fsys/ap_file.h>
#include <ap_fsys/stdio_agent.h>
#include <ap_fsys/proc_fs.h>

struct condition{
    int is_open;
    pthread_cond_t cond;
    struct ger_stem_node stem;
    int v;
    pthread_mutex_t cond_lock;
}cond;

static int cond_open(struct ger_stem_node *stem, unsigned long flag)
{
    struct condition *cond = container_of(stem, struct condition, stem);
    if (cond->is_open == 0) {
        pthread_cond_init(&cond->cond, NULL);
        pthread_mutex_init(&cond->cond_lock, NULL);
        cond->v = 0;
    }
    return 0;
}

static ssize_t cond_write(struct ger_stem_node *stem, char *buf, off_t off, size_t size)
{
    struct condition *cond = container_of(stem, struct condition, stem);
    pthread_mutex_lock(&cond->cond_lock);
    cond->v = 1;
    pthread_mutex_unlock(&cond->cond_lock);
    pthread_cond_signal(&cond->cond);
    return 1;
}

static int cond_destory(struct ger_stem_node *stem)
{
    struct condition *cond = container_of(stem, struct condition, stem);
    pthread_cond_destroy(&cond->cond);
    pthread_mutex_destroy(&cond->cond_lock);
    cond->is_open = 0;
    cond->v = 0;
    return 0;
}

static ssize_t cond_read(struct ger_stem_node *stem, char *buf, off_t off, size_t size)
{
    struct condition *cond = container_of(stem, struct condition, stem);
    do {
        pthread_mutex_lock(&cond->cond_lock);
        if (cond->v != 0) {
            pthread_mutex_unlock(&cond->cond_lock);
            break;
        }
        pthread_cond_wait(&cond->cond, &cond->cond_lock);
    }while (cond->v == 0);
    pthread_mutex_unlock(&cond->cond_lock);
    return cond->v;
}

static struct stem_file_operations cond_file_ops = {
    .stem_read = cond_read,
    .stem_write = cond_write,
    .stem_open = cond_open,
};

static struct stem_inode_operations cond_inode_ops = {
    .stem_destory = cond_destory,
};

int main(int argc, const char * argv[]) {
    /*initiate ger and proc file system*/
    ap_file_thread_init();
    init_fs_ger();
    init_proc_fs();
    int mount_s;
    struct std_age_dir *root_dir;
    /*crate test directory*/
    int mk_s = mkdir("/tmp/ap_test_proc", S_IRWXU | S_IRWXG | S_IRWXO);
    if (mk_s == -1) {
        perror("mkdir failed\n");
        exit(1);
    }
    
    mk_s = mkdir("/tmp/ap_test_proc/proc_sub_dir", S_IRWXU | S_IRWXG | S_IRWXO);
    if (mk_s == -1) {
        perror("mkdir failed\n");
        exit(1);
    }
    
    open("/tmp/ap_test_proc/proc_sub_dir/sub_test", O_CREAT | O_RDWR);
    
    /*fill the test-file*/
    char pre_write_buf[] = "test_proc\n";
    int fd = open("/tmp/ap_test_proc/proc_test", O_CREAT | O_RDWR);
    write(fd, pre_write_buf, sizeof(pre_write_buf));
    chmod("/tmp/ap_test_proc/proc_test", S_IRWXU | S_IRWXG | S_IRWXO);
    close(fd);
    
    /*import /tmp/ap_test_proc into this process as root directory "/"*/
    root_dir = MALLOC_STD_AGE_DIR("/tmp/ap_test_proc");
    root_dir->stem.stem_name = "/";
    root_dir->stem.stem_mode = 0777;
    
    /*mount ger file system*/
    mount_s = ap_mount(&root_dir->stem, GER_FILE_FS, "/");
    if (mount_s == -1)
        perror("mount failed\n");
    
    /*mount proc file system*/
    struct proc_mount_info m_info;
    m_info.typ = SYSTEM_V;
    m_info.sever_name = "proc_user1";
    mount_s = ap_mount(&m_info, PROC_FILE_FS, "/procs");
    
    if (mount_s == -1)
        perror("mount failed\n");
    
    cond.is_open = 0;
    cond.stem.stem_name = "condition";
    cond.stem.stem_mode = 0777;
    cond.stem.sf_ops = &cond_file_ops;
    cond.stem.si_ops = &cond_inode_ops;
    
    struct ger_stem_node *root = find_stem_p("/");
    if (root == NULL) {
        perror("root not found\n");
        exit(1);
    }
    int hook_s = hook_to_stem(root, &cond.stem);
    if (hook_s == -1) {
        perror("hook failed\n");
        exit(1);
    }
    
    int cond_fd = ap_open("/condition", O_RDWR);
    if (cond_fd == -1) {
        perror("open failed\n");
        exit(1);
    }
    
    char v;
    ssize_t read_n = ap_read(cond_fd, &v, 1);
    if (read_n == -1) {
        perror("read cond failed\n");
        exit(1);
    }
}