/*
 *   Copyright (c) 2015, HU XUKAI
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License.
 *
 */
#ifndef ap_file_system_module_h
#define ap_file_system_module_h
#define MODULE_NAME_MAX 100
#include <pthread.h>
#include <counter.h>
#include <list.h>
#include <ger_fs.h>

#define SIG_NEW_MOD 10
#define MODULE_LAYOUT_MARK 0x8086
#define MODULE_DIR_PATH ".ap_modules"
/*
 *since we use linux kernel module for convenience, hence we need to know
 *the layout of the kernel verison of struct module
 */
struct kernel_module_layout{
	unsigned int mark;
	int is_layout_set;
	unsigned int name_off;
	
	unsigned int init_off;	/*off set of init function*/
	unsigned int exit_off;	/*off set of exit function*/
}linux_mod_layout;

struct module_indic{
	void *module;
	void *init;
	void *exit;
	void *name;
};

struct sym_search{
	void *start;
	void *end;
	size_t num_sym;
};

typedef int (*mod_init)(void);
typedef void (*mod_exit)(void);

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
	
	struct list_head mod_global_lis;
	struct list_head mod_wait_excute;
	struct list_head mod_using;
};

struct module_use {
	struct module *mode;
	struct list_head module_which_using;
};

static inline struct module_use *MALLOC_MOD_USE()
{
	struct module_use *mu;
	mu = Malloc_z(sizeof(*mu));
	INIT_LIST_HEAD(&mu->module_which_using);
	return mu;
}

static inline struct module *MALLOC_MOD(){
	struct module *mode;
	mode = Malloc_z(sizeof(*mode));
	INIT_LIST_HEAD(&mode->mod_global_lis);
	INIT_LIST_HEAD(&mode->mod_wait_excute);
	INIT_LIST_HEAD(&mode->mod_using);
	COUNTER_INIT(&mode->mod_counter);
	return mode;
}


struct module_ex_file_struct{
	struct ger_stem_node node;
	struct module *mode;
};

static inline struct module_ex_file_struct *MALLOC_M_EX_FILE()
{
	struct module_ex_file_struct *m_ex_f = Malloc_z(sizeof(*m_ex_f));
	STEM_INIT(&m_ex_f->node);
	return m_ex_f;
}

struct module_global{
	struct list_head m_g_lis;
	pthread_mutex_t m_g_lock;
};

extern struct module_global mode_global;

struct module_wait{
	int is_thr_weak;
	pthread_cond_t wait_cond; /*indicate wether there are modules wait to be excuted*/
	struct list_head m_excuting;
	int wait_n; /*the number of module wait in the queue*/
	struct list_head m_need_excute;
	pthread_mutex_t m_n_lock;
};

extern struct module_wait mode_wait;

static inline void module_wait_excute(struct module *mod)
{
	pthread_mutex_lock(&mode_wait.m_n_lock);
	list_add_tail(&mod->mod_wait_excute, &mode_wait.m_need_excute);
	mode_wait.wait_n++;
	if (mode_wait.wait_n == 1 && mode_wait.is_thr_weak == 0) {
		pthread_mutex_unlock(&mode_wait.m_n_lock);
		pthread_cond_signal(&mode_wait.wait_cond);
	}
	pthread_mutex_unlock(&mode_wait.m_n_lock);
}

static inline void module_add_to_global(struct module *mod)
{
	pthread_mutex_lock(&mode_global.m_g_lock);
	list_add(&mod->mod_global_lis, &mode_global.m_g_lis);
	pthread_mutex_unlock(&mode_global.m_g_lock);
}

struct porc_syms{
	struct sym_search se;
	pthread_mutex_t se_lock;
	int get;
};

extern struct porc_syms k_syms;

struct ap_symbol;
extern struct module *load_module(void *buff, unsigned long len);
extern int module_free(struct module *mode);
#endif
