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
static struct stem_inode_operations module_ex_inode_operation;
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
	new_node->stem_mode = 0755;
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
	if (kill(getpid(),SIG_NEW_MOD)) {
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
	addr = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
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
	counter_get(&mod->mod_counter);
	
	/*ok, we can exute the new module*/

	kick_new_module();
	return 0;
}

static int module_ex_unlink(struct ger_stem_node *node)
{
	struct module_ex_file_struct *file = container_of(node, struct module_ex_file_struct, node);
	int r_s = module_free(file->mode);
	return r_s;
}

static void add_module_file(const char *name, struct module *mode)
{
	struct module_ex_file_struct *m_ex;
	struct ger_stem_node *node = find_stem_p("/modules/mode_ex_list");
	if (node == NULL) {
		ap_err("mode_ex_list was lost!\n");
		exit(1);
	}
	m_ex = MALLOC_M_EX_FILE();
	char *f_name = Malloc_z(strlen(name) + 1);
	strcmp(f_name, name);
	
	m_ex->node.stem_name = f_name;
	m_ex->node.si_ops = &module_ex_inode_operation;
	m_ex->node.stem_mode = 0750;
	m_ex->mode = mode;
	counter_get(&mode->mod_counter);
	hook_to_stem(node, &m_ex->node);
	return;
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
		/*add a file to module_ex_list*/
		add_module_file(mod->module_name, mod);
		
		list_del(&mod->mod_wait_excute);
		counter_put(&mod->mod_counter);
	}
}
static char *get_proc_name()
{
	int fd;
	pid_t pid = getpid();
	char buff[20];
	char path[100];
	memset(path,'\0', 100);
	memset(buff,'\0', 20);
	sprintf(buff, "%d",pid);
	const char *paths[] = {
		"/proc",
		buff,
		"comm",
	};
	/*read from /proc/"pid"/comm */
	path_names_cat(path, paths, 3, "/");
	fd = open(path, O_RDONLY);
	if (fd == -1) {
		ap_err("can't find process name!");
		return NULL;
	}
	char *proc_name = Malloc_z(100);
	if(read(fd, proc_name, 100) <= 0){
		ap_err("can't read from proc file!");
		return NULL;
	}
	char **cut = cut_str(proc_name, '\n', 1);
	return cut[0];
}

static void module_file_prepare(struct ger_stem_node *node)
{
	char path[200];
	memset(path, 0, 200);
	struct std_age_dir *module_dir;
	struct ger_stem_node *module_ex_dir;
	
	/*set signal which is rasied when new module is loaded*/
	sig_t ss = signal(SIG_NEW_MOD, excute_new_module);
	if (ss == SIG_ERR) {
		ap_err("set SIG_NEW_MOD failed\n");
		exit(1);	
	}

	/*load file, write module path to it when you want to load new module*/
	struct ger_stem_node *load_node = MALLOC_STEM();
	load_node->sf_ops = &module_file_operation;
	load_node->stem_mode = 0755;
	load_node->stem_name = "load_module";
	int hook_s = hook_to_stem(node, load_node);
	if(hook_s == -1) {
		ap_err("hook failed\n");
		return;	
	}
	/*
	 *find or create module directory in which all the modules related
	 *to this process are stored
	 */
	char *home = getenv("HOME");
	if (home == NULL) {
		ap_err("can't find home dir!\n");
		return;
	}
	const char *module_dir_path[] = {
		home,
		MODULE_DIR_PATH,	
	};

	path_names_cat(path, module_dir_path, 2, "/");
	
	int mk_s = mkdir(path, 0755);
	if (mk_s == -1 && errno != EEXIST) {
		ap_err("can't make module direcoty or find it!");
		exit(1);
	}
	
	char *proc_name = get_proc_name();
	path_name_cat(path, proc_name, strlen(proc_name), "/");

	mk_s = mkdir(path, 0750);
	if (mk_s == -1 && errno != EEXIST) {
		ap_err("can't make process module dir!\n");
		exit(1);
	}
	/*module binary file list*/
	module_dir = MALLOC_STD_AGE_DIR(path);
	module_dir->stem.stem_name = "mod_list";
	module_dir->stem.parent = node;
	module_dir->stem.stem_mode = 0750;
	hook_to_stem(node, &module_dir->stem);
	
	/*module was loaded*/
	module_ex_dir = MALLOC_STEM();
	module_ex_dir->is_dir = 1;
	module_ex_dir->stem_name = "mode_ex_list";
	module_ex_dir->parent = node;
	module_ex_dir->stem_mode = 0750;
	hook_to_stem(node, module_ex_dir);
	
	/*find dependence file and import it*/
	path_name_cat(path, "dependence", 10, "/");
	int fd = open(path, O_RDWR | O_CREAT);
	close(fd);
}

static struct stem_file_operations module_file_operation = {
	.stem_write = module_stem_write,
};

static struct stem_inode_operations module_ex_inode_operation = {
	.stem_unlink = module_ex_unlink,
};
