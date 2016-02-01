#include <elf.h>
#include <module.h>
#include <envelop.h>
#include <string.h>
#include <ap_string.h>
#include <export.h>
/*the code is similar with the code of linux kernel in kernel/module.c*/
#define	BITS_PER_LONG (sizeof(unsigned long) * 8)
#define K_ALIGN(x, a) __ALIGN(x, (typeof(x))(a) - 1)
#define __ALIGN(x, mask) (((x) + (mask)) & ~mask)
#define INIT_OFFSET_MASK (1UL << (BITS_PER_LONG-1))

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

static void section_layout(struct load_info *info)
{
#define CHECK_NUM 4
	
	static unsigned long const masks[CHECK_NUM][2] = {
		{ SHF_EXECINSTR | SHF_ALLOC, 0 },
		{ SHF_ALLOC, SHF_WRITE | 0 },
		{ SHF_WRITE | SHF_ALLOC, 0 },
		{ 0 | SHF_ALLOC, 0 }
	};
	
	char *sname;
	struct module *mod = info->mod;
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
		shdr->sh_entsize = (get_off_set(&mod->init_size, shdr)
					| INIT_OFFSET_MASK);
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

static int is_core_symb(const Elf_Sym *sym, const Elf_Shdr *shdr,
			unsigned int sh_num)
{
	const Elf_Shdr *sec;
	
	if (sym->st_shndx == SHN_UNDEF
		|| sym->st_shndx > sh_num
		|| !sym->st_name)
		return 0;
	sec = shdr + sym->st_shndx;
	if (!(sec->sh_flags & SHF_ALLOC)
		|| (sec->sh_entsize & INIT_OFFSET_MASK))
		return 0;
	return 1;
}

static void symtab_layout(struct load_info *info)
{
	struct module *mod = info->mod;
	Elf_Shdr *symsec = info->shdr + info->index.sym;
	Elf_Shdr *symstrsec = info->shdr + info->index.str;
	Elf_Sym *sym = (void *)info->ehdr + symsec->sh_offset;
	unsigned int i, nsym, ncores, strb_size = 0;
	
	symsec->sh_flags |= SHF_ALLOC;
	symsec->sh_entsize = get_off_set(&mod->init_size, symsec);
	nsym = (unsigned int)symsec->sh_size / sizeof(*sym);
	for (i = ncores = 0; i < nsym; i++) {
		if (is_core_symb(sym + i, info->shdr, info->ehdr->e_shnum)){
			strb_size += strlen(info->strtb + sym[i].st_name);
			ncores++;
		}
	}
	mod->sym_off = K_ALIGN(mod->core_size, symsec->sh_addralign);
	mod->str_off = mod->sym_off + ncores * sizeof(Elf_Sym);
	mod->core_size = mod->str_off;
	
	symstrsec->sh_flags &= SHF_ALLOC;
	symstrsec->sh_entsize = get_off_set(&mod->init_size, symstrsec);

	return;
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
			info->strtb = (void *)info->ehdr
				+ info->shdr[info->index.str].sh_offset;
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
	
	return 0;
}

static struct kernel_module_layout get_kernel_module_layout()
{
	
}

static int sec_copy_to_dest(struct load_info *info)
{
	void *ptr;
	
	ptr = Malloc_z(info->mod->core_size);
	info->mod->module_core = ptr;
	
	if (info->mod->init_size) {
		ptr = Malloc_z(info->mod->init_size);
		info->mod->module_init = ptr;
	}else
		info->mod->module_init = NULL;
	for (unsigned int i = 0; i < info->ehdr->e_shnum; i++) {
		void *dest = NULL;
		Elf_Shdr *shdr = &info->shdr[i];

		if ((shdr->sh_flags & SHF_ALLOC) != SHF_ALLOC)
			continue;
		if (shdr->sh_entsize & INIT_OFFSET_MASK)
			dest = info->mod->module_init
				+ (shdr->sh_entsize & ~INIT_OFFSET_MASK);
		else
			dest = info->mod->module_core + shdr->sh_entsize;
		if (shdr->sh_type != SHT_NOBITS)
			memcpy(dest, (void *)shdr->sh_addr, shdr->sh_size);
		
		shdr->sh_addr = (unsigned long)dest;
	}
	return 0;
}

static struct ap_symbol *find_symbol(const char *name)
{
	struct sym_search *search = get_ap_symbol();
	struct ap_symbol *sym = search->start;
	size_t strl= strlen(name);
	unsigned int sym_num = (unsigned int) (search->start - search->end)
						/ sizeof(*sym);
	
	for (unsigned int i = 0; i < sym_num; i++) {
		if (strncmp(sym[i].name, name, strl) == 0 )
			return sym + i;
	}
	
	return NULL;
}

static int fix_symbols(struct load_info *info)
{
	Elf_Shdr *sym_shdr = &info->shdr[info->index.sym];
	Elf_Sym *sym = (void *)sym_shdr->sh_addr;
	struct ap_symbol *ap_symbol;
	
	int ret = 0;
	
	for (unsigned int i = 0; i < (sym_shdr->sh_size / sizeof(Elf_Sym)); i++) {
		const char *name = info->strtb + sym[i].st_name;
		
		switch (sym[i].st_shndx) {
		case SHN_COMMON:
			if (!strncmp(name, "__gnu_lto", 9))
				break;
				
			fprintf(stderr, "%s:please compile with -fno-common\n",
					info->mod->module_name);
			ret = ENOEXEC;
			break;
		case SHN_UNDEF:
			ap_symbol = find_symbol(name);
		}
	}
	
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
	
	section_layout(info);
	symtab_layout(info);
	if (sec_copy_to_dest(info))
		return NULL;
	
	
FREE:
	
	
	
	
}