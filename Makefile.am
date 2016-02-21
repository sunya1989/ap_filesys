pkglib_LTLIBRARIES = libapfsys.la
libapfsys_la_SOURCES = src/ger/ger_fs.c src/protocols/system_v.c \
					 src/protocols/ap_ipc_protocols.c src/thread_angent/thread_agent.c \
					 src/ap_file_thread.c src/ap_file.c src/ap_fs.c \
					 stdio_age/stdio_agent.c proc/proc_fs.h proc/proc_fs.c \
					 ap_stdlib/convert.c ap_stdlib/ap_hash.c ap_stdlib/bag.c ap_stdlib/counter.c \
					 ap_stdlib/ap_string.c ap_stdlib/lock_reg.c ap_stdlib/bitmap.c src/lib_export.c \
					 src/module_agent.c src/module.c

pkginclude_HEADERS = include/ap_file.h include/counter.h include/ger_fs.h include/list.h include/stdio_agent.h \
					 include/thread_agent.h include/bag.h include/proc_fs.h include/ap_types.h include/envelop.h \
					 include/export.h include/module_layout.h include/lib_export.h

libapfsys_la_CPPFLAGS = -I$(top_srcdir)/src/src_include