#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdio.h>
#include <module.h>
#include <ap_file.h>
#include <ger_fs.h>

static struct stem_file_operations module_file_operation;


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
	new_node->sf_ops = &module_file_operation;
	hook_to_stem(node, new_node);
	
	return 0;
}

static int kick_new_module()
{
	
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
	
	pthread_mutex_lock(&mode_wait.m_n_lock);
	list_add(&mod->mod_wait_excute, &mode_wait.m_need_excute);
	pthread_mutex_unlock(&mode_wait.m_n_lock);
	
	/*ok, we can exute the new module*/
	int k = kick_new_module();
	if (k == -1)
		return -1;
	
	return 0;
}

static struct stem_file_operations module_file_operation = {
	.stem_write = module_stem_write,
};


