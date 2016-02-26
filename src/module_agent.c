#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdio.h>
#include <module.h>
#include <ap_file.h>
#include <ger_fs.h>
#include <ap_string.h>
#include <stdio_agent.h>

static struct stem_file_operations module_file_operation;
static void module_file_prepare(struct ger_stem_node *node);
int mount_module_agent()
{
	struct ger_stem_node *node = find_stem_p("/");
	if (node == NULL) {
		perror("can't find root!\n");
		errno = ENOENT;
		return -1;
	}
	struct ger_stem_node *new_node = MALLOC_STEM();
	new_node->stem_name = "modules";
	new_node->stem_mode = 0666;
	new_node->is_dir = 1;
	new_node->prepare_raw_data = module_file_prepare;
	
	int hook_s = hook_to_stem(node, new_node);
	if (hook_s == -1) {
		ap_err("hook error");
		return -1;
	}
	return 0;
}

static void kick_new_module()
{
	if (raise(SIG_NEW_MOD)) {
		perror("sig_new_mod wrong!");
		exit(1);
	}
}

static ssize_t module_stem_write(struct ger_stem_node *node,
								char *buff,
								off_t off,
								size_t size)
{
	void *addr;
	struct stat st;
	struct module *mod;
	
	/*open module file path*/
	int fd = open(buff, O_RDONLY);
	if (fd == -1)
		return -1;
	
	if (fstat(fd, &st) == -1)
		return -1;
	
	/*map module file into address space*/
	addr = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (addr == MAP_FAILED)
		return -1;
	
	/*check load new module*/
	mod = load_module(addr, st.st_size);
	if (mod == NULL)
		return -1;
	
	if (munmap(addr, st.st_size) == -1) {
		ap_err("umap module failed\n");
		exit(1);
	}
	
	pthread_mutex_lock(&mode_wait.m_n_lock);
	list_add(&mod->mod_wait_excute, &mode_wait.m_need_excute);
	pthread_mutex_unlock(&mode_wait.m_n_lock);
	
	/*ok, we can exute the new module*/
	kick_new_module();
	return 0;
}

static void excute_new_module(int sig)
{
	struct list_head *pos;
	struct module *mod;
	
	/*iterate through all new loaded modules*/
	list_for_each(pos, &mode_wait.m_need_excute){
		mod = list_entry(pos, struct module, mod_wait_excute);
		if (mod->init == NULL) {
			perror("module init miss!\n");
			exit(1);
		}
		if (mod->init()) {
			printf("%s module init failed\n", mod->module_name);
		}
	}
}
static char *get_proc_name()
{
	FILE *fs;
	pid_t pid = getpid();
	char buff[20];
	char path[100];
	sprintf(buff, "%d",pid);
	const char *paths[] = {
		"proc",
		buff,
		"comm",
	};
	/*read from /proc/"pid"/comm */
	path_names_cat(path, paths, 3, "/");
	fs = fopen(path, "r");
	if (fs == NULL) {
		ap_err("can't find process name!");
		return NULL;
	}
	char *proc_name = Malloc_z(100);
	if(fgets(proc_name, 100, fs) == NULL){
		ap_err("can't read from proc file!");
		return NULL;
	}
	
	return proc_name;
}

static void module_file_prepare(struct ger_stem_node *node)
{
	char path[200];
	memset(path, 0, 200);
	struct std_age_dir *module_dir;
	
	/*set signal which is rasied when new module is loaded*/
	signal(SIG_NEW_MOD, excute_new_module);
	
	/*load file, write module path to it when you want to load new module*/
	struct ger_stem_node *load_node = MALLOC_STEM();
	load_node->sf_ops = &module_file_operation;
	load_node->stem_mode = 0666;
	load_node->stem_name = "load_module";
	hook_to_stem(node, load_node);
	
	/*
	 *find or create module directory in which all the modules related
	 *to this process are stored
	 */
	int mk_s = mkdir(MODULE_DIR_PATH, 0666);
	if (mk_s == -1) {
		ap_err("can't make module direcoty or find it!");
		exit(1);
	}
	
	char *proc_name = get_proc_name();
	const char *paths[] = {
		MODULE_DIR_PATH,
		proc_name,
	};
	
	path_names_cat(path, paths, 2, "/");
	mk_s = mkdir(path, 0660);
	if (mk_s == -1) {
		ap_err("");
		exit(1);
	}
	module_dir = MALLOC_STD_AGE_DIR(path);
	module_dir->stem.stem_name = "mod_list";
	module_dir->stem.parent = node;
	module_dir->stem.stem_mode = 0660;
	hook_to_stem(node, &module_dir->stem);
	
	/*find dependence file and import it*/
	const char *dep_path[] = {
		"dependence"
	};
	
	path_names_cat(path, dep_path, 2, "/");
	int fd = open(path, O_RDWR | O_CREAT);
	close(fd);
}

static struct stem_file_operations module_file_operation = {
	.stem_write = module_stem_write,
};
