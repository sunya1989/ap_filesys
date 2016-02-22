
#ifndef export_h
#define export_h
/*
 * Export symbols from the kernel to modules.  Forked from module.h
 * to reduce the amount of pointless cruft we feed to gcc when only
 * exporting a simple symbol or two.
 *
 * Try not to add #includes here.  It slows compilation and makes kernel
 * hackers place grumpy comments in header files.
 */

/* Some toolchains use a `_' prefix for all user symbols. */
#ifdef CONFIG_HAVE_UNDERSCORE_SYMBOL_PREFIX
#define __VMLINUX_SYMBOL(x) _##x
#define __VMLINUX_SYMBOL_STR(x) "_" #x
#else
#define __VMLINUX_SYMBOL(x) x
#define __VMLINUX_SYMBOL_STR(x) #x
#endif

#define __visible	__attribute__((externally_visible))

/* Indirect, so macros are expanded before pasting. */
#define VMLINUX_SYMBOL(x) __VMLINUX_SYMBOL(x)
#define VMLINUX_SYMBOL_STR(x) __VMLINUX_SYMBOL_STR(x)
struct ap_symbol
{
	unsigned long value;
	const char *name;
};

/* Mark the CRC weak since genksyms apparently decides not to
 * generate a checksums for some symbols */
#define __CRC_SYMBOL(sym, sec)					\
extern __visible void *__crc_##sym __attribute__((weak));		\
static const unsigned long __kcrctab_##sym		\
__attribute__((__used__))							\
__attribute__((section("___kcrctab" sec "+" #sym), unused))	\
= (unsigned long) &__crc_##sym;

/* For every exported symbol, place a struct in the __ksymtab section */
#define __EXPORT_SYMBOL(sym, sec)				\
extern typeof(sym) sym;					\
__CRC_SYMBOL(sym, sec)					\
static const char __kstrtab_##sym[]			\
__attribute__((section("__ksymtab_strings"), aligned(1))) \
= VMLINUX_SYMBOL_STR(sym);				\
extern const struct ap_symbol __ksymtab_##sym;	\
__visible const struct ap_symbol __ksymtab_##sym	\
__attribute__((__used__))							\
__attribute__((section("___ksymtab" sec "+" #sym), unused))	\
= { (unsigned long)&sym, __kstrtab_##sym }

#define EXPORT_SYMBOL(sym)					\
__EXPORT_SYMBOL(sym, "")

#endif /* export_h */
