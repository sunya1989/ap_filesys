#ifndef ap_file_system_module_h
#define ap_file_system_module_h
#define MODULE_NAME_MAX 100
#include <counter.h>
#include <list.h>
/*
 *since we use linux kernel module for convenience, hence we need to know
 *the layout of the kernel verison of struct module
 */
struct kernel_module_layout{
	unsigned long init_off;	/*off set of init function*/
	unsigned long exit_off;	/*off set of exit function*/
};

struct sym_search{
	void *start;
	void *end;
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
	
	void *syms;
	
	struct list_head modules_dominate_lis;
};

struct module_dominate{
	struct list_head list;
	struct module *module_dominated;
};
struct ap_symbol;
extern struct sym_search *get_ap_symbol();


#endif