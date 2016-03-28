/*   
 *This source code is released for free distribution under the terms of the
 *GNU General Public License.
 */
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <elf.h>
#include <module.h>
#include <envelop.h>
#include <string.h>
#include <ap_string.h>
#include <export.h>
#include <ap_file.h>
/*the code is similar with the code of linux kernel in kernel/module.c*/
#define	BITS_PER_LONG (sizeof(unsigned long) * 8)
#define K_ALIGN(x, a) __ALIGN(x, (typeof(x))(a) - 1)
#define __ALIGN(x, mask) (((x) + (mask)) & ~mask)
#define INIT_OFFSET_MASK (1UL << (BITS_PER_LONG-1))

struct module_global mode_global = {
	.m_g_lis = LIST_HEAD_INIT(mode_global.m_g_lis),
	.m_g_lock = PTHREAD_MUTEX_INITIALIZER,
};

struct module_wait mode_wait = {
	.wait_cond = PTHREAD_COND_INITIALIZER,
	.m_excuting = LIST_HEAD_INIT(mode_wait.m_excuting),
	.m_need_excute = LIST_HEAD_INIT(mode_wait.m_need_excute),
	.m_n_lock = PTHREAD_MUTEX_INITIALIZER,
};

struct porc_syms k_syms = {
	.se_lock = PTHREAD_MUTEX_INITIALIZER,
};

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
		|| (info->ehdr->e_type != ET_REL && info->ehdr->e_type != ET_EXEC)
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
		shdr->sh_addr = (size_t)info->ehdr + shdr->sh_offset;
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
		for (int j = 0; j < info->ehdr->e_shnum; j++){
			Elf_Shdr *shdr = &info->shdr[j];
			sname = info->secstring + shdr->sh_name;
		
			if ((shdr->sh_flags & masks[i][0]) != masks[i][0]
				|| (shdr->sh_flags & masks[i][1])
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

static int prepare_load_info(struct load_info *info, unsigned long len)
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

static int get_kernel_module_layout(struct load_info *info)
{
	if (linux_mod_layout.is_layout_set)
		return 0;
	
	unsigned int mod_inc = find_sec(info, ".ap_module_indicator");
	if (!mod_inc)
		return -1;
	Elf_Shdr *shdr = &info->shdr[mod_inc];
	struct module_indic *m_i = (void *)shdr->sh_addr;
	linux_mod_layout.init_off = (unsigned int)(m_i->init - m_i->module);
	linux_mod_layout.exit_off = (unsigned int)(m_i->exit - m_i->module);
	linux_mod_layout.name_off = (unsigned int)(m_i->name - m_i->module);
	return 0;
}

static int sec_copy_to_dest(struct load_info *info)
{
	void *ptr;
	
	size_t page_size = sysconf(_SC_PAGESIZE);
	int al = posix_memalign(&ptr, page_size, info->mod->core_size);
	if (al) {
		errno = al;
		ap_err("allocate memory for module failed!\n");
	}
	info->mod->module_core = ptr;
	
	if (info->mod->init_size) {
		al = posix_memalign(&ptr, page_size, info->mod->init_size);
		if (al) {
			errno = al;
			ap_err("allocate memory for module failed!\n");
		}
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

static struct sym_search *get_ap_symbol()
{
	if (k_syms.get)
		return &k_syms.se;
	
	pthread_mutex_lock(&k_syms.se_lock);
	if (k_syms.get) {
		pthread_mutex_unlock(&k_syms.se_lock);
		return &k_syms.se;
	}
	
#define MAX_LINE 255
	char *buff;
	struct stat st;
	void *addr;
	struct load_info *info = Malloc_z(sizeof(*info));
	struct sym_search *ss = Malloc_z(sizeof(*info));
	Elf_Shdr *hdr;
	ss->end = ss->start = NULL;
	ss->num_sym = 0;
	
	/*get the process file path*/
	buff = getenv("PROG_PATH");
	if (buff == NULL) {
		ap_err("can't find env PROG_PATH!\n");
		errno = ENOEXEC;
		return NULL;
	}
	
	/*get file status*/
	int fd = open(buff, O_RDONLY);
	if (fd == -1)
		return NULL;
	
	if (fstat(fd, &st) == -1)
		return NULL;
	
	/*map the file into address space*/
	addr = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (addr == MAP_FAILED)
		return NULL;
	
	/*check the sanity of elf file*/
	info->ehdr = addr;
	info->shdr = (void *)info->ehdr + info->ehdr->e_shoff;
	info->len = st.st_size;
	info->secstring = (void *)info->ehdr + info->shdr[info->ehdr->e_shstrndx].sh_offset;
	
	if (elf_check(info))
		return NULL;

	/*find __ksymtab section*/
	int sec = find_sec(info, "___ksymtab");
	if (sec == 0)
		return ss;
	
	hdr = &info->shdr[sec];
	if (info->len < hdr->sh_offset + hdr->sh_size) {
		errno = ENOEXEC;
		perror("file has been truncated");
		return NULL;
	}
	hdr->sh_addr = (size_t)info->ehdr + hdr->sh_offset;
	/*move sym table*/
	void *new_addr = Malloc_z(hdr->sh_size);
	memcpy(new_addr, (void *)hdr->sh_addr, hdr->sh_size);
	k_syms.se.start = new_addr;
	k_syms.se.end = new_addr + hdr->sh_size;
	k_syms.se.num_sym = hdr->sh_size / sizeof(struct ap_symbol);
	k_syms.get = 1;
	pthread_mutex_unlock(&k_syms.se_lock);
	int unmap_s = munmap(addr, st.st_size);
	if (unmap_s == -1) {
		ap_err("umap source file failed!\n");
		exit(1);
	}
	
	return &k_syms.se;
}

static struct ap_symbol *find_symbol_in_module(struct module *mod,
											   const char *name)
{
	unsigned long num = mod->syms_num;
	size_t strl = strlen(name);
	for (unsigned long i = 0 ; i < num; i++) {
		if (strncmp(mod->syms[i].name, name, strl) == 0)
			return mod->syms + i;
	}
	
	return NULL;
}

static struct ap_symbol *find_symbol(const char *name)
{
	struct sym_search *search = get_ap_symbol();
	if (search == NULL) {
		errno = ENOENT;
		ap_err("no symbol in the source!\n");
		return NULL;	
	}
	struct list_head *pos;
	struct module *pos_p;
	struct ap_symbol *sym = search->start;

	if (search->num_sym) {
		size_t sym_num = search->num_sym;
		for (unsigned int i = 0; i < sym_num; i++) {
			if (strncmp(sym[i].name, name, strlen(sym[i].name)) == 0 )
				return sym + i;
		}
	}

	pthread_mutex_lock(&mode_global.m_g_lock);
	list_for_each(pos, &mode_global.m_g_lis){
		pos_p = container_of(pos, struct module, mod_global_lis);
		if ((sym = find_symbol_in_module(pos_p, name)) != NULL)
			return sym;
	}
	pthread_mutex_unlock(&mode_global.m_g_lock);
	return NULL;
}

static int fix_symbols(struct load_info *info)
{
	errno = 0;
	Elf_Shdr *sym_shdr = &info->shdr[info->index.sym];
	Elf_Sym *sym = (void *)sym_shdr->sh_addr;
	struct ap_symbol *ap_symbol;
	unsigned long base = 0;
	
	
	for (unsigned int i = 0; i < (sym_shdr->sh_size / sizeof(Elf_Sym)); i++) {
		const char *name = info->strtb + sym[i].st_name;
		
		switch (sym[i].st_shndx) {
		case SHN_COMMON:
			if (!strncmp(name, "__gnu_lto", 9))
				break;
				
			fprintf(stderr, "%s:please compile with -fno-common\n",
					info->mod->module_name);
			errno = ENOEXEC;
			break;
		case SHN_ABS:
			fprintf(stderr, "Absolute symbol\n");
			break;
		case SHN_UNDEF:
			ap_symbol = find_symbol(name);
			
			if (ap_symbol) {
				sym[i].st_value = ap_symbol->value;
				break;
			}
			
			if (!ap_symbol && (ELF_ST_BIND(sym[i].st_info) == STB_WEAK 
						|| ELF_ST_TYPE(sym[i].st_info) == STT_NOTYPE))
				break;
			fprintf(stderr, "%s: Unknown symbol %s (err %i)\n",
					info->mod->module_name,name,errno);
			errno = errno ?: ENOENT;
			break;
		default:
			base = info->shdr[sym[i].st_shndx].sh_addr;
			sym[i].st_value += base;
		}
	}
	return errno ? -1:0;
}

static int apply_relocate_add(Elf64_Shdr *sechdrs,
					   const char *strtab,
					   unsigned int symindex,
					   unsigned int relsec,
					   struct module *me)
{
	unsigned int i;
	Elf64_Rela *rel = (void *)sechdrs[relsec].sh_addr;
	Elf64_Sym *sym;
	void *loc;
	__u64 val;
	
	for (i = 0; i < sechdrs[relsec].sh_size / sizeof(*rel); i++) {
		/* This is where to make the change */
		loc = (void *)sechdrs[sechdrs[relsec].sh_info].sh_addr
		+ rel[i].r_offset;
		
		/* This is the symbol it is referring to.  Note that all
		 undefined symbols have been resolved.  */
		sym = (Elf64_Sym *)sechdrs[symindex].sh_addr
		+ ELF64_R_SYM(rel[i].r_info);
		
		val = sym->st_value + rel[i].r_addend;
		
		switch (ELF64_R_TYPE(rel[i].r_info)) {
			case R_X86_64_NONE:
				break;
			case R_X86_64_64:
				*(__u64 *)loc = val;
				break;
			case R_X86_64_32:
				*(__u32 *)loc = (__u32)val;
				if (val != *(__u32 *)loc)
					goto overflow;
				break;
			case R_X86_64_32S:
				*(__s32 *)loc = (__s32)val;
				if ((__s64)val != *(__s32 *)loc)
					goto overflow;
				break;
			case R_X86_64_PC32:
				val -= (__u64)loc;
				*(__u32 *)loc = (__u32)val;
				
				break;
			default:
				fprintf(stderr,"%s: Unknown rela relocation: %lu\n",
					   me->module_name, ELF64_R_TYPE(rel[i].r_info));
				errno = ENOEXEC;
				return -1;
		}
	}
	return 0;
	
overflow:
	fprintf(stderr,"overflow in relocation type %d val %lx\n",
		   (int)ELF64_R_TYPE(rel[i].r_info), val);
	fprintf(stderr,"`%s' likely not compiled with -mcmodel=kernel\n",
		   me->module_name);
	errno = ENOEXEC;
	return -1;
}

static int apply_relocations(const struct load_info *info)
{
	struct module *mod = info->mod;
	errno = 0;
	for (unsigned int i = 0; i < info->ehdr->e_shnum ; i++) {
		int sec_info = info->shdr[i].sh_info;
		
		if (sec_info > info->ehdr->e_shnum)
			continue;
		if (!(info->shdr[sec_info].sh_flags & SHF_ALLOC))
			continue;
		if (info->shdr[i].sh_type == SHT_REL) {
			/*only support x86-64 right now*/
			fprintf(stderr, "module %s relocation unsurpport\n", mod->module_name);
			errno = ENOEXEC;
			return -1;
		}
		if (info->shdr[i].sh_type == SHT_RELA) {
			if (apply_relocate_add(info->shdr, info->strtb,
								   info->index.sym, i, mod))
				return -1;
		}
	}
	return 0;
}

static int verify_export_symbol(struct module *mod)
{
	unsigned long num = mod->syms_num;
	for (unsigned long i = 0; i < num; i++) {
		if (find_symbol(mod->syms[i].name) != NULL)
			return -1;
	}
	return 0;
}

static int find_mod_syms(struct load_info *info)
{
	unsigned int sec = find_sec(info, "__ksymtab");
	Elf_Shdr *hdr = &info->shdr[sec];
	info->mod->syms = (void *)hdr->sh_addr;
	info->mod->syms_num = hdr->sh_size / sizeof(*info->mod->syms);
	return 0;
}

static struct module *find_real_module(struct load_info *info,
									   struct kernel_module_layout kml)
{
	Elf_Shdr *hdr = &info->shdr[info->index.mod];
	info->mod->init = *((mod_init *)((void *)hdr->sh_addr + kml.init_off));
	info->mod->exit = *((mod_exit *)((void *)hdr->sh_addr + kml.exit_off));
	strncpy(info->mod->module_name, (char *)hdr->sh_addr
			+ kml.name_off, MODULE_NAME_MAX);
	find_mod_syms(info);
	return info->mod;
}

/*
 *load module, return module structure with point to init and exit function
 */
struct module *load_module(void *buff, unsigned long len)
{
	struct load_info *info = Malloc_z(sizeof(*info));
	info->ehdr = buff;
	info->len = len;
	if (elf_check(info))
		goto FREE;
	
	if (prepare_load_info(info, len))
		goto FREE;
	
	section_layout(info);
	if (sec_copy_to_dest(info))
		 goto FREE;
	if (fix_symbols(info))
		goto FREE;

	int a_s = apply_relocations(info);
	if (a_s == -1) {
		errno = ENOEXEC;
		ap_err("relocation failed\n!");
		return NULL;			
	}

	int ly_s = get_kernel_module_layout(info);
	if (ly_s == -1) {
		perror("Module layout is ambiguous!");
		errno = ENOEXEC;
		return NULL;
	}
	
	/*
	 *change access permission of allocated memeory in
	 *where the module code was placed.
	 *the value of module_core and module_init have already been 
	 *aligned with page size
	 */
	int mp_s = mprotect(info->mod->module_core, info->mod->core_size, PROT_EXEC | PROT_READ | PROT_WRITE);
	if (mp_s == -1) {
		perror("protect failed");
		exit(1);	
	}

	mp_s = mprotect(info->mod->module_init, info->mod->init_size, PROT_EXEC | PROT_READ | PROT_WRITE);
	if (mp_s == -1) {
		perror("protect failed");
		exit(1);	
	}

	info->mod = find_real_module(info, linux_mod_layout);
	if (info->mod == NULL) {
		errno = ENOEXEC;
		goto FREE;
	}
	
	if (verify_export_symbol(info->mod))
		goto FREE;
	
	module_add_to_global(info->mod);
	return info->mod;
	
FREE:
	if (info->mod->core_size && info->mod->module_core)
		free(info->mod->module_core);
	if (info->mod->init_size && info->mod->module_init)
		free(info->mod->module_init);
	return NULL;
}

int module_free(struct module *mode)
{
	pthread_mutex_lock(&mode_global.m_g_lock);
	if (mode->mod_counter.in_use > 0)
		return -1;
	list_del(&mode->mod_global_lis);
	pthread_mutex_unlock(&mode_global.m_g_lock);
	/*say good bye!*/
	mode->exit();
	
	if (mode->core_ro_size && mode->module_core)
		free(mode->module_core);
	if (mode->init_size && mode->module_init)
		free(mode->module_init);
	return 0;
}


