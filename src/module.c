#include <elf.h>
#include <module.h>
#include <envelop.h>
#include <string.h>
#include <ap_string.h>
/*the code is similar with the code of linux kernel in kernel/module.c*/
#define K_ALIGN(x, a) __ALIGN(x, (typeof(x))(a) - 1)
#define __ALIGN(x, mask) (((x) + (mask)) & ~mask)

struct  load_info{
	Elf_Ehdr *ehdr;
	Elf_Shdr *shdr;
	char *secstring, *strtb;
	unsigned long len;
	struct module *mod;
	struct {
		unsigned int sym, str, mod, info, vers;
	}index;
};

static long get_off_set(unsigned long *size, Elf_Shdr *section)
{
	long tmp;
	tmp = K_ALIGN(*size, section->sh_addralign ?:1);
	*size = tmp + section->sh_size;
	return tmp;
}

static int find_sec(struct load_info *info, char *sec_name)
{
	for (unsigned int i = 0; i < info->ehdr->e_shnum; i++) {
		Elf_Shdr *shdr = &info->shdr[i];
		if (shdr->sh_flags & SHF_ALLOC) {
			char *tmp_sec_name = info->secstring + shdr->sh_name;
			if (strcmp(tmp_sec_name, sec_name) == 0)
				return i;
		}
	}
	return 0;
}

static int elf_check(struct load_info *info)
{
	if (info->len < sizeof(*info->ehdr)) {
		errno = ENOEXEC;
		return -1;
	}
	
	if (memcmp(info->ehdr->e_ident, ELFMAG, SELFMAG) != 0
		|| info->ehdr->e_type != ET_REL
		|| !elf_check_arch(info->ehdr)
		|| info->ehdr->e_shentsize != sizeof(Elf_Shdr)){
		errno = ENOEXEC;
		return -1;
	}
	if (info->ehdr->e_shoff >= info->len
		|| (info->ehdr->e_shnum  * info->ehdr->e_shentsize) >
		info->len - info->ehdr->e_shoff) {
		errno = ENOEXEC;
		return -1;
	}
	return 0;
}

static int check_rewrite_shdr(struct load_info *info)
{
	info->shdr[0].sh_addr = 0;
	for (unsigned int i = 0; i < info->ehdr->e_shnum; i++) {
		Elf_Shdr *shdr = &info->shdr[i];
		if (shdr->sh_type != SHT_NOBITS
			&& info->len < shdr->sh_offset + shdr->sh_size) {
			fprintf(stderr,"Module len %lu truncated\n",info->len);
			errno = ENOEXEC;
			return -1;
		}
		shdr->sh_addr = (size_t)info->ehdr + info->shdr->sh_offset;
	}
	
	info->index.vers = find_sec(info, "__versions");
	info->index.info = find_sec(info, ".modinfo");
	info->shdr[info->index.info].sh_flags &= ~SHF_ALLOC;
	info->shdr[info->index.vers].sh_flags &= ~SHF_ALLOC;
	return 0;
}

static void section_layout(struct module *mod, struct load_info *info)
{
#define CHECK_NUM 4
	
	static unsigned long const masks[CHECK_NUM][2] = {
		{ SHF_EXECINSTR | SHF_ALLOC, 0 },
		{ SHF_ALLOC, SHF_WRITE | 0 },
		{ SHF_WRITE | SHF_ALLOC, 0 },
		{ 0 | SHF_ALLOC, 0 }
	};
	
	char *sname;
	
	for (int i = 0; i < info->ehdr->e_shnum; i++)
		info->shdr[i].sh_entsize = ~0UL;
		
	
	for (int i = 0; i < CHECK_NUM; i++) {
		for (int j = 0; j < info->ehdr->e_shnum; j++) {
			Elf_Shdr *shdr = &info->shdr[j];
			sname = info->secstring + shdr->sh_name;
			
			if ((shdr->sh_flags & masks[i][0]) != masks[i][0]
				|| (shdr->sh_flags & masks[i][1])
				|| (shdr->sh_entsize != ~0UL)
				|| (strstarts(sname, ".init")))
				continue;
			shdr->sh_entsize = get_off_set(&mod->core_size, shdr);
			switch (i) {
			case 0:
				mod->core_text_size = mod->core_size;
				break;
			case 1:
				mod->core_ro_size = mod->core_ro_size;
				break;
			}
		}
	}
	
	for (int i = 0; i < CHECK_NUM; i++) {
		Elf_Shdr *shdr = &info->shdr[i];
		sname = info->secstring + shdr->sh_name;
		
		if ((shdr->sh_flags & masks[i][0]) != masks[i][0]
			|| (shdr->sh_flags & masks[i][0])
			|| (shdr->sh_entsize != ~0UL)
			|| !(strstarts(sname, ".init")))
			continue;
		shdr->sh_entsize = get_off_set(&mod->init_size, shdr);
		switch (i) {
		case 0:
			mod->init_text_size = mod->init_size;
			break;
		case 1:
			mod->init_ro_size = mod->init_size;
		break;
		}
	}
	
}

static int prepare_load_info(struct load_info *info, unsigned long len,
						struct kernel_module_layout ker_lay)
{
	info->shdr = (void *)info->ehdr + info->ehdr->e_shoff;
	info->secstring = (void *)info->ehdr + info->shdr[info->ehdr->e_shstrndx].sh_offset;
	info->len = len;
	
	long err = check_rewrite_shdr(info);
	if (err != 0)
		return -1;
	
	for (int i = 0; i < info->ehdr->e_shnum; i++) {
		if (info->shdr[i].sh_type == SHT_SYMTAB) {
			info->index.sym = i;
			info->index.str = info->shdr[i].sh_link;
			info->strtb = (void *)info->ehdr + info->shdr[i].sh_offset;
			break;
		}
	}
	
	info->index.mod = find_sec(info, ".gnu.linkonce.this_module");
	
	if (!info->index.mod){
		perror("No Module found\n");
		errno = ENOEXEC;
		return -1;
	}
	if (!info->index.sym){
		perror("Module has no symbols\n");
		errno = ENOEXEC;
		return -1;
	}
	
	info->mod = Malloc_z(sizeof(*info->mod));
	info->mod->init = (void *)info->ehdr + info->shdr[info->index.mod].sh_offset + ker_lay.init_off;
	info->mod->exit = (void *)info->ehdr + info->shdr[info->index.mod].sh_offset + ker_lay.exit_off;
	
	return 0;
}

static struct kernel_module_layout get_kernel_module_layout()
{
	
}

/*
 *load module, return module structure with point to init and exit function
 */
struct module *load_module(void *buff, unsigned long len)
{
	struct load_info *info = Malloc_z(sizeof(*info));
	info->ehdr = buff;
	
	if (elf_check(info))
		goto FREE;
	
	struct kernel_module_layout kml = get_kernel_module_layout();
	if (prepare_load_info(info, len, kml))
		goto FREE;
	
	
FREE:
	
	
	
	
}