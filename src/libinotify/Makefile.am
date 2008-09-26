include $(top_srcdir)/Makefile.decl

INCLUDES =						\
	-DG_LOG_DOMAIN=\"libinotify\"			\
	-I$(top_srcdir)/src				\
	$(GLIB2_CFLAGS)

noinst_LTLIBRARIES = libinotify.la

libinotify_la_SOURCES = 				\
	inotify-monitor.c 				\
	inotify-handle.c 				\
	inotify-listhash.c 				\
	inotify-monitor.h				\
	inotify-handle.h 				\
	inotify-listhash.h 				\
	inotify-log.h 					\
	linux-inotify-syscalls.h			\
	libinotify.h

libinotify_la_LDFLAGS = -version-info 0:0:0
libinotify_la_LIBADD = 					\
	$(GLIB2_LIBS)
