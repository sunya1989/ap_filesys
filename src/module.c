#include <elf.h>
#include <module.h>
#include <envelop.h>


struct  load_info{
	Elf_Ehdr *ehdr;
	Elf_Shdr *shdr;
	unsigned long len;
};

static int module_check()

static struct  load_info *prepare_load_info(void *buff)
{
	struct load_info *l_in = Malloc_z(sizeof(*l_in));
	
}

struct module *load_module(void *buff, unsigned long len)
{


}