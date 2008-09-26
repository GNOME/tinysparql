include $(top_srcdir)/Makefile.decl

INCLUDES = \
	-D_XOPEN_SOURCE_EXTENDED=1 \
	-D_GNU_SOURCE=1	\
	-D__EXTENSIONS__=1 \
	-D_HPUX_SOURCE=1 \
	-D_POSIX_MAPPED_FILES=1	\
	-D_POSIX_SYNCHRONIZED_IO=1 \
	-DPIC=1 \
	-D_THREAD_SAFE=1 \
	-D_REENTRANT=1 \
	-DNDEBUG \
	-DMYNOMMAP 

noinst_LTLIBRARIES = libqdbm-private.la

libqdbm_private_la_SOURCES = \
	depot.c \
	depot.h \
	myconf.c \
	myconf.h
