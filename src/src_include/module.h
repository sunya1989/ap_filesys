#ifndef ap_file_system_module_h
#define ap_file_system_module_h
#define MODULE_NAME_MAX 100

/*
 *since we use linux kernel module for convenience, hence we need to know
 *the layout of the kernel verison of struct module
 */
struct kernel_module_layout{
	unsigned long init_off;	/*off set of init function*/
	unsigned long exit_off;	/*off set of exit function*/
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
};

#endif