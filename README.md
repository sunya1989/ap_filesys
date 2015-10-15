1.	AP_FSYS is a VFS(virtual file system)that inside a process， one can use it to organise data and features of a process

1.1 file system is consist of a class of information units and operations upon these units. for an instance, taking a picture as a information unit, then read and modify(write) this picture constitute operations on this picture. file system is also responsible for organising these informations (usually in the form of tree).

2. 	AP_FSYS use structure ap_inode to represent a information unit, use ap_file_operations and ap_inode_operations represent operatons on that unit.(they are defined in ap_fs.h)

3.gernal file system is a file system that satisfy 1.1. it has implemented ap_file_operations and ap_inode_operations, furthermore it maps ap_inode to ger_stem_node and maps ap_file_operations and ap_inode_operations to stem_file_operations and stem_inode_operations respectively(the definitions of ger_stem_node stem_file_operations and stem_inode_operations are in ger_fs.h)

4.one can embed ger_stem_node into an arbitrary structure, and also can treat this structure as a "file" once the stem_file_operations and stem_inode_operations hanve been implemented. for a instance

－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－exmaples.c
	struct gernal{
    int is_open;
    pthread_mutex_t lock;
    size_t size;
    
    struct ger_stem_node stem;
    
    char buf[TEST_LINE_MAX];
}ger_ex;
－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－

stem is a instance of structure ger_stem_node that is embeded into ger_ex. the operations is following

－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－exmaples.c
static ssize_t ger_ex_read(struct ger_stem_node *stem, char *buf, off_t off, size_t size)
{
    /*recover the object into which the stem was embedded*/
    struct gernal *ger = container_of(stem, struct gernal, stem);
   
   						......... 
    
    pthread_mutex_lock(&ger->lock);
    /*for the simplicity we just ignore the off parameter*/
    memcpy(buf, ger->buf, size);
    pthread_mutex_unlock(&ger->lock);
    
    return size;
}

static ssize_t ger_ex_write(struct ger_stem_node *stem, char *buf, off_t off, size_t size)
{   
									......... 

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
－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－

we need to mount gernal file system before it can acctually work

－－－－－－－－－－－－－－－－－－－－－－－－－－－－－-－－－－－exmaples.c
int mount_s = ap_mount(&root_dir->stem, GER_FILE_FS, "/");
－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－

－－－－－－－－－－－－－－－－－－－－－－－－－－－－－exmaples.c
/*pointing to the file and inode operations*/
    ger_ex.stem.sf_ops = &ger_ex_file_ops;
    ger_ex.stem.si_ops = &ger_ex_inode_ops;
    
    ger_ex.stem.stem_mode = 0777;
    ger_ex.stem.stem_name = "gernal_exmple";
								
			.........

	int hook_s = hook_to_stem(root, &ger_ex.stem);
－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－

after hook_to_stem ger_ex is added to gernal file system then you can open read write it

－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－exmaples.c
/*open the file*/
    int fd = ap_open("/gernal_exmple", O_RDWR);							

          .........

    char buf[TEST_LINE_MAX];
    memset(buf, '\0', TEST_LINE_MAX);
    
    ssize_t read_n = ap_read(fd, buf, TEST_LINE_MAX);
    			
    	  .........
    
    printf("%s\n",buf);
							
		  .........

	char write_buf[] = "writing....";
    ssize_t write_n = ap_write(fd, write_buf, sizeof(write_buf));
－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－

5. stdio agent is based on gernal file system, use stdio agent one can import directories and files that resides in OS into process. first establish a dirctory and a file

－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－exmaples.c
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
－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－

then import this dirctory into process as root dirctory

－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－exmaples.c
/*import /tmp/aptest into this process as root directory "/"*/
    root_dir = MALLOC_STD_AGE_DIR("/tmp/ap_test");
    root_dir->stem.stem_name = "/";
    root_dir->stem.stem_mode = 0777;

/*mount "/"*/
    int mount_s = ap_mount(&root_dir->stem, GER_FILE_FS, "/");
－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－

like we have shown above one can open read write the file that just have been import, desipite "/tmp/ap_test" has become root dirctory in the process
－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－exmaples.c
 int ap_fd = ap_open("/std_test", O_RDWR);
    
     .........    
    
    /*read test-file*/
    char buf[TEST_LINE_MAX];
    memset(buf, '\0', TEST_LINE_MAX);
    ssize_t read_n = ap_read(ap_fd, buf, TEST_LINE_MAX);

     .........
    	
    printf("%s\n",buf);
    
    /*write test-file*/
    char write_buf[] = "stdio has been worte\n";
    ssize_t write_n = ap_write(ap_fd, write_buf, sizeof(write_buf))

     .........
－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－

6.thread agent is also based on gernal file system, i want use it to export the attribues of a thread originally examples of thread agent is in exmaples.c

7.proc file system is attempt to establish connection between two processes. for a instance, process A use AP_FSYS create a directory-file structure then process B can use proc file system access this structure inside process A.

first mount the proc file system

－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－exmaples.c
 /*prepare mount information*/
    struct proc_mount_info info;
    info.sever_name = "proc_user0";
    info.typ = SYSTEM_V;
    
    /*mount file system*/
    int mount_s = ap_mount(&info, PROC_FILE_FS, "/procs");
－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－
info is the information that mount process need, sever_name is the sever name you choose for the process info.typ is the type of IPC you choose.

meanwhile mount proc file system in other process
first mount the root dirctory
－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－proc_example.c
 /*import /tmp/ap_test_proc into this process as root directory "/"*/
    root_dir = MALLOC_STD_AGE_DIR("/tmp/ap_test_proc");
    root_dir->stem.stem_name = "/";
    root_dir->stem.stem_mode = 0777;
    
    /*mount ger file system*/
    mount_s = ap_mount(&root_dir->stem, GER_FILE_FS, "/");
－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－
the root directory is imported "/tmp/ap_test_proc"

then mount the proc file system
－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－proc_example.c
 /*mount proc file system*/
    struct proc_mount_info m_info;
    m_info.typ = SYSTEM_V;
    m_info.sever_name = "proc_user1";
    mount_s = ap_mount(&m_info, PROC_FILE_FS, "/procs");
－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－
as we can see the sever name of this process is "proc_user1"

after that we can acess the file of proc_usr1 through proc_usr0
by the way we have also defined a condition structure as bellow

－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－proc_example.c
struct condition{
    int is_open;
    pthread_cond_t cond;

    struct ger_stem_node stem;
    
    int v;
    pthread_mutex_t cond_lock;
}cond;
－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－

this structure is for the testing has a condition variable and cond to the gernal file system

－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－proc_example.c
cond.stem.stem_name = "condition";
cond.stem.stem_mode = 0777;
cond.stem.sf_ops = &cond_file_ops;
cond.stem.si_ops = &cond_inode_ops;
   			 .........
int hook_s = hook_to_stem(root, &cond.stem);
－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－

open /conditon and read from it will make process wait in that condition variable
－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－proc_example.c
 int cond_fd = ap_open("/condition", O_RDWR);

   			 .........
	char v;
	ssize_t read_n = ap_read(cond_fd, &v, 1);
－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－
when some other process have complete some work then write condition file will make proc_usr1 return from waiting

now we can find dirctory that relate to proc_usr1 usually has the form /procs/severname/severname@pid

－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－exmaples.c
 /*open directory and read*/
    dir = ap_open_dir("/procs");
    if (dir == NULL) {
        perror("open dir failed\n");
        exit(1);
    }
    /*prepare the path*/
    strcat(proc_path, "/procs");
    while (loop < 2) {
        strncat(proc_path, "/", 1);
        dirt = NULL;
        dirt = ap_readdir(dir);
        if (dirt == NULL){
            printf("dirctory is empty!\n");
            exit(0);
        }
        strncat(proc_path, dirt->name, dirt->name_l);
        ap_closedir(dir);
        
        dir = NULL;
        dir = ap_open_dir(proc_path);
        if (dir == NULL && errno != ENOTDIR) {
            perror("open dir failed\n");
            exit(1);
        }
        loop++;
    }
－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－

the code above is looking for /procs/proc_usr1/proc_usr1@pid 

then search condition file and another file under /procs/proc_usr1/proc_usr1@pid
－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－exmaples.c
while (!c_file_find || !t_file_find) {
        dirt = NULL;
        dirt = ap_readdir(dir);
        if (dirt == NULL){
            printf("dirctory is empty!\n");
            exit(0);
        }
        if (strncmp(dirt->name, "condition", dirt->name_l) == 0) {
            c_file_find = 1;
            strncat(cond_path, dirt->name, dirt->name_l);
        }else{
            t_file_find = 1;
            strncat(proc_path, dirt->name, dirt->name_l);
        }
    }
－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－

read and write the file that you have found

－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－exmaples.c
 /*open the file that reside in other process*/
    fd1 = ap_open(proc_path, O_RDWR);
   
     			 .........
 
 /*write*/
 char *write_w = "proc_user0 have wrote\n";
 size_t len = strlen(write_w);
 ssize_t write_n = ap_write(fd1, write_w, len);
     			 
     			 .........

	/*read*/
 ap_lseek(fd1, 0, SEEK_SET);
 ssize_t read_n = ap_read(fd1, &test_r, TEST_LINE_MAX);			
－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－

open codition file write this weak up proc_usr1 
－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－proc_example.c
	/*open conditon file*/
	int fd2 = ap_open(cond_path, O_RDWR);
    
 					    .........
	char v;
	write_n = ap_write(fd2, &v, 1);
－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－





