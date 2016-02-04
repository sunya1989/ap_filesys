#ifndef ap_file_system_module_h
#define ap_file_system_module_h
#define MODULE_NAME_MAX 100
#include <pthread.h>
#include <counter.h>
#include <list.h>
/*
 *since we use linux kernel module for convenience, hence we need to know
 *the layout of the kernel verison of struct module
 */
struct kernel_module_layout{
	unsigned int name_off;
	
	unsigned int init_off;	/*off set of init function*/
	unsigned int exit_off;	/*off set of exit function*/
};

struct sym_search{
	void *start;
	void *end;
	size_t num_sym;
};

struct module{
	char module_name[MODULE_NAME_MAX];
	
	int (*init) (void);
	void (*exit)(void);
	
	unsigned long core_size;
	unsigned long core_text_size;
	unsigned long core_ro_size;
	unsigned long init_size;
	unsigned long init_text_size;
	unsigned long init_ro_size;
	unsigned long sym_off;
	unsigned long str_off;
	
	struct counter mod_counter;
	
	void *module_core;
	void *module_init;
	
	struct ap_symbol *syms;
	unsigned long syms_num;
	
	struct list_head mod_dominate_lis;
	struct list_head mod_global_lis;
};

struct module_dominate{
	struct list_head list;
	struct module *module_dominated;
};

struct module_global{
	struct list_head m_g_lis;
	pthread_mutex_t m_g_lock;
}mode_global;

static void module_add_to_global(struct module *mod)
{
	pthread_mutex_lock(&mode_global.m_g_lock);
	list_add(&mod->mod_global_lis, &mode_global.m_g_lis);
	pthread_mutex_unlock(&mode_global.m_g_lock);
}

struct porc_syms{
	struct sym_search se;
	pthread_mutex_t se_lock;
	int get;
}k_syms;

struct ap_symbol;
extern struct sym_search *get_ap_symbol();


#endif