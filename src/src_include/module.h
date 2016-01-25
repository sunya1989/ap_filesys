#ifndef ap_file_system_module_h
#define ap_file_system_module_h
#define MODULE_NAME_MAX 100




struct module{
	char module_name[MODULE_NAME_MAX];
	int (*init) (void);
	void (*exit)(void);
};

#endif