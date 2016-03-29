//
//  module_example.c
//  examples
//

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <ap_fsys/ap_file.h>
#include <ap_fsys/ger_fs.h>
#include <ap_fsys/stdio_agent.h>
#include <ap_fsys/lib_export.h>

static void module_example()
{
#define MODULE_PATH ""
	if(MODULE_PATH == "") {
		printf("oops, MODULE_PATH is NULL, there is a module example source code under examples/ap_module_ex.\n \
			you need compile it by yourself (there is already a Makefile under same folder,all you need to do is \n\
			 just make), and replace MODULE_PATH with the path of newly compiled moudle example binary file\n");
		exit(1);	
	}
	if(mount_module_agent() == -1){
		ap_err("module agent failed!\n");
		exit(1);
	}
	
	int fd = ap_open("/modules/load_module", O_WRONLY);
	if (fd == -1) {
		ap_err("can't open module load file!\n");
		exit(1);
	}
	
	printf("load module %s", MODULE_PATH);
	ssize_t w_s = ap_write(fd, MODULE_PATH, strlen(MODULE_PATH));
	if (w_s == -1) {
		ap_err("module can't be loaded!\n");
		exit(1);
	}
	/*wait for the module file is mounted*/
	sleep(10);
	/*remove module*/
	AP_DIR *dir = ap_open_dir("/modules/mode_ex_list");
	if (dir == NULL) {
		perror("open failed!\n");
		exit(1);
	}
	ap_chdir("/modules/mode_ex_list");
	struct ap_dirent *dirt;
	while ((dirt = ap_readdir(dir))!= NULL) {
		ap_unlink(dirt->name);
	}
}

int main(int argc, const char * argv[])
{  /*initiate for current thread*/
	ap_file_thread_init();
	
	/*initiate ger file system*/
	init_fs_ger();
	struct std_age_dir *root_dir;
	
	/*make a test-directory*/
	int mk_s = mkdir("/tmp/ap_test", S_IRWXU | S_IRWXG | S_IRWXO);
	if (mk_s == -1 && errno != EEXIST) {
		perror("make new test dir failed\n");
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
	
	
	char *prog = realpath(argv[0],NULL);
	printf("prog path :%s",prog);
	int ss = setenv( "PROG_PATH", prog, 1);
	if (ss == -1) {
		ap_err("set prog path failed!\n");
		exit(1);
	}
	
	module_example();
return 0;
}
