/*
 *   Copyright (c) 2015, HU XUKAI
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <ap_fsys/ap_types.h>
#include <ap_fsys/ap_file.h>
#include <ap_fsys/ger_fs.h>
#include <ap_fsys/stdio_agent.h>
#include <ap_fsys/thread_agent.h>
#include <ap_fsys/proc_fs.h>

#define TEST_LINE_MAX 100
#define TEST_PATH_LEN 200

/*thread test-struct*/
struct attr_test{
    pthread_mutex_t test_lock;
    int size;
    char buf[TEST_LINE_MAX];
}attr_t;
static struct thread_attr_operations thread_ops;

/*thread read function*/
static ssize_t test_attr_read(void *buff, struct ger_stem_node*stem, off_t off_t, size_t size)
{
    if (size > TEST_LINE_MAX)
        size = TEST_LINE_MAX;
    
    pthread_mutex_lock(&attr_t.test_lock);
    memcpy(buff, attr_t.buf, size);
    pthread_mutex_unlock(&attr_t.test_lock);
    return size;
}

/*thread write function*/
static ssize_t test_attr_write(void *buff, struct ger_stem_node*stem, off_t off_t, size_t size)
{
    if (size > TEST_LINE_MAX)
        size = TEST_LINE_MAX;
    
    pthread_mutex_lock(&attr_t.test_lock);
    memcpy(attr_t.buf, buff, size);
    pthread_mutex_unlock(&attr_t.test_lock);
    return size;
}

static struct thread_attr_operations thread_ops = {
    .attr_read = test_attr_read,
    .attr_write = test_attr_write,
};

/*thead to print the content of attr_t.buf*/
void *__test_thread_age_print(void *a)
{
    printf("print\n");
    char c;
    ssize_t read_n;
    
    /*initiate for current thread*/
    ap_file_thread_init();
    
    int fd = ap_open("/thread_test/test0", O_RDONLY);
    if (fd == -1) {
        perror("thread open failed\n");
        exit(1);
    }
    
    /*read and print*/
    for (int i=0; i < 100;) {
        if (attr_t.size == 0) {
            continue;
        }
        read_n = ap_read(fd, &c, 1);
        if (read_n == -1) {
            perror("threan read failed\n");
            exit(1);
        }
        printf("%c",c);
        i++;
    }
    pthread_exit(NULL);
}

/*write content into attr.buf*/
void *__test_thread_age_w1(void *a)
{
    printf("W1\n");
    /*initiate for current thread*/
    ap_file_thread_init();
    int fd = ap_open("/thread_test/test0", O_WRONLY);
    printf("w1 fd:%d\n",fd);
    
    if (fd == -1) {
        perror("th_w1 failed\n");
    }
    for (int i=0; i<100; i++) {
        ap_write(fd, "a", 1);
        attr_t.size++;
    }
    ap_close(fd);
    pthread_exit(NULL);
}

void *__test_thread_age_w2(void *a)
{
    printf("W2\n");
    ap_file_thread_init();
    int fd = ap_open("/thread_test/test0", O_WRONLY);
    printf("w2 fd:%d\n",fd);
    if (fd == -1) {
        perror("th_w2 failed\n");
    }
    for (int i=0; i<100; i++) {
        ap_write(fd, "b", 1);
        attr_t.size++;
    }
    ap_close(fd);
    pthread_exit(NULL);
}

void thread_exmple()
{
    pthread_t n0, n1, n2;
    void *n_p0, *n_p1, *n_p2;
    
    /*initiate attr*/
    attr_t.size = 0;
    
    /*create */
    struct thread_age_dir *dir = MALLOC_THREAD_AGE_DIR("thread_test");
    struct thread_age_attribute *attr = MALLOC_THREAD_AGE_ATTR(&thread_ops, "test0");
    attr->thr_stem.stem_mode = 0777;
    dir->thr_dir_stem.stem_mode = 0777;
    
    /*hook file(attr) to the directory(dir)*/
    int hook_s = hook_to_stem(&dir->thr_dir_stem, &attr->thr_stem);
    if (hook_s == -1) {
        perror("hook failed\n");
        exit(1);
    }
    
    /*find the directory to wich you want hook dir*/
    struct ger_stem_node *root = find_stem_p("/");
    if (root == NULL) {
        printf("faile to find stem");
        exit(1);
    }
    
    /*hook the dir*/
    hook_s = hook_to_stem(root, &dir->thr_dir_stem);
    if (hook_s == -1) {
        perror("hook failed\n");
        exit(1);
    }
    
    /*create threads*/
    pthread_create(&n0, NULL, __test_thread_age_print, NULL);
    pthread_create(&n1, NULL, __test_thread_age_w1, NULL);
    pthread_create(&n2, NULL, __test_thread_age_w2, NULL);
    
    pthread_join(n0, &n_p0);
    pthread_join(n1, &n_p1);
    pthread_join(n2, &n_p2);
    
    /*unlink file*/
    ap_unlink("/thread_test/test0");
    /*remove dir*/
    ap_rmdir("/thread_test");
}

struct gernal{
    int is_open;
    pthread_mutex_t lock;
    size_t size;
    struct ger_stem_node stem;
    char buf[TEST_LINE_MAX];
}ger_ex;

static ssize_t ger_ex_read(struct ger_stem_node *stem, char *buf, off_t off, size_t size)
{
    /*recover the object into which the stem was embedded*/
    struct gernal *ger = container_of(stem, struct gernal, stem);
    if (ger->size == 0)
        return  0;
    if (size > ger->size)
        size = ger->size;
    
    pthread_mutex_lock(&ger->lock);
    /*for the simplicity we just ignore the off parameter*/
    memcpy(buf, ger->buf, size);
    pthread_mutex_unlock(&ger->lock);
    
    return size;
}

static ssize_t ger_ex_write(struct ger_stem_node *stem, char *buf, off_t off, size_t size)
{
    if (size > TEST_LINE_MAX)
        size = TEST_LINE_MAX;
    /*recover the object into which the stem was embedded*/
    struct gernal *ger = container_of(stem, struct gernal, stem);
    pthread_mutex_lock(&ger->lock);
    /*for the simplicity we just ignore the off parameter*/
    memcpy(ger->buf, buf, size);
    pthread_mutex_unlock(&ger->lock);
    
    return size;
}

static int ger_ex_open(struct ger_stem_node *stem, unsigned long flag)
{
    /*recover the object into which the stem was embedded*/
    struct gernal *ger = container_of(stem, struct gernal, stem);
    
    /*if have not been opened before then initiate ger_ex*/
    if (!ger->is_open) {
        pthread_mutex_init(&ger->lock, NULL);
        ger->is_open = 1;
    }
    
    ger->size = 0;
    memset(ger->buf, '\0', TEST_LINE_MAX);
    /*fill some contents*/
    char text[] = "ger_ex is opened";
    strncpy(ger->buf, text, sizeof(text));
    ger->size = sizeof(text);
    return 0;
}

static int ger_ex_destory(struct ger_stem_node *stem)
{
    /*recover the object into which the stem was embedded*/
    struct gernal *ger = container_of(stem, struct gernal, stem);
    
    memset(ger->buf, '\0', TEST_LINE_MAX);
    pthread_mutex_destroy(&ger->lock);
    ger->size = 0;
    return 0;
}

static struct stem_file_operations ger_ex_file_ops = {
    .stem_read = ger_ex_read,
    .stem_write = ger_ex_write,
    .stem_open = ger_ex_open,
};

static struct stem_inode_operations ger_ex_inode_ops = {
    .stem_destory = ger_ex_destory,
};

static void ger_exmple()
{
    ger_ex.is_open = 0;
    /*pointing to the file and inode operations*/
    ger_ex.stem.sf_ops = &ger_ex_file_ops;
    ger_ex.stem.si_ops = &ger_ex_inode_ops;
    
    ger_ex.stem.stem_mode = 0777;
    ger_ex.stem.stem_name = "gernal_exmple";
    
    /*find the directory to wich you want hook ger_tem*/
    struct ger_stem_node *root = find_stem_p("/");
    if (root == NULL) {
        printf("faile to find stem");
        exit(1);
    }
    
    int hook_s = hook_to_stem(root, &ger_ex.stem);
    if (hook_s == -1) {
        perror("hook failed\n");
        exit(1);
    }
    
    /*open the file*/
    int fd = ap_open("/gernal_exmple", O_RDWR);
    if (fd == -1) {
        perror("gernal open failed\n");
        exit(1);
    }
    char buf[TEST_LINE_MAX];
    memset(buf, '\0', TEST_LINE_MAX);
    
    ssize_t read_n = ap_read(fd, buf, TEST_LINE_MAX);
    if (read_n == -1) {
        perror("gernal read fialed\n");
        exit(1);
    }
    printf("%s\n",buf);
    
    /*write some thing*/
    char write_buf[] = "writing....";
    ssize_t write_n = ap_write(fd, write_buf, sizeof(write_buf));
    if (write_n == -1) {
        perror("gernal write failed\n");
        exit(1);
    }
    
    /*read what just have been wrote*/
    memset(buf, '\0', TEST_LINE_MAX);
    read_n = ap_read(fd, buf, TEST_LINE_MAX);
    if (read_n == -1) {
        perror("gernal read failed\n");
        exit(1);
    }
    printf("%s\n", buf);
    
    /*close the file*/
    ap_close(fd);
    ap_unlink("/gernal_exmple");
    return;
}

static void proc_example()
{
    /*initiate for current thread*/
    ap_file_thread_init();
    
    /*initiate ger file system*/
    init_fs_ger();
    struct std_age_dir *root_dir;
    
    /*make a test-directory*/
    int mk_s = mkdir("/tmp/ap_test", S_IRWXU | S_IRWXG | S_IRWXO);
    if (mk_s == -1) {
        perror("");
        exit(1);
    }
    
    /*fill the test-file*/
    char pre_write_buf[] = "test_stdio\n";
    int fd = open("/tmp/ap_test/std_test", O_CREAT | O_RDWR);
    write(fd, pre_write_buf, sizeof(pre_write_buf));
    chmod("/tmp/ap_test/std_test", 0777);
    close(fd);
    
    /*import /tmp/aptest into this process as root directory "/"*/
    root_dir = MALLOC_STD_AGE_DIR("/tmp/ap_test");
    root_dir->stem.stem_name = "/";
    root_dir->stem.stem_mode = 0777;
    
    /*mount "/"*/
    int mount_s = ap_mount(&root_dir->stem, GER_FILE_FS, "/");
    if (mount_s == -1) {
        perror("mount root failed\n");
        exit(1);
    }
    
    /*open test-file*/
    int ap_fd = ap_open("/std_test", O_RDWR);
    if (ap_fd == -1) {
        perror("open failed\n");
        exit(1);
    }
    
    /*read test-file*/
    char buf[TEST_LINE_MAX];
    memset(buf, '\0', TEST_LINE_MAX);
    ssize_t read_n = ap_read(ap_fd, buf, TEST_LINE_MAX);
    if (read_n == -1) {
        perror("read failed\n");
        exit(1);
    }
    printf("%s\n",buf);
    
    /*write test-file*/
    char write_buf[] = "stdio has been worte\n";
    ssize_t write_n = ap_write(ap_fd, write_buf, sizeof(write_buf));
    if (write_n == -1) {
        perror("write failed\n");
        exit(-1);
    }
    
    /*close file*/
    int close = ap_close(ap_fd);
    if (close == -1) {
        perror("close failed\n");
        exit(1);
    }
    
    /*link /std_test to /std_test_link*/
    int link_s = ap_link("/std_test", "/std_test_link");
    if (link_s == -1) {
        perror("link failed\n");
        exit(1);
    }
    
    fd = ap_open("/std_test_link", O_RDWR);
    if (fd == -1) {
        perror("open link-test-file failed\n");
        exit(1);
    }
    
    memset(buf, '\0', TEST_LINE_MAX);
    read_n = ap_read(fd, buf, TEST_LINE_MAX);
    if (read_n == -1) {
        perror("read link-test-file failed\n");
        exit(1);
    }
    printf("%s", buf);
    ap_close(fd);
    
    int unl_s = ap_unlink("/std_test_link");
    if (unl_s == -1) {
        perror("unlink failed\n");
        exit(1);
    }
    
    /*demonstrate gernal example*/
    ger_exmple();
    
    /*demonstrate thread agent*/
    thread_exmple();
    
    /*demonstrate proc example*/
    proc_example();
}






