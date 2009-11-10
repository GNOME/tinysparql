include $(top_srcdir)/Makefile.decl

module_flags = -module -avoid-version -no-undefined
modulesdir = $(libdir)/tracker-$(TRACKER_API_VERSION)/writeback-modules

INCLUDES = 								\
	-DLOCALEDIR=\""$(localedir)"\" 					\
	-DMODULESDIR=\"$(modulesdir)\"					\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTRACKER_COMPILATION						\
	-DWRITEBACK_MODULES_DIR=\""$(modulesdir)"\"			\
	-I$(top_srcdir)/src 						\
	$(WARN_CFLAGS)							\
	$(GLIB2_CFLAGS)							\
	$(GCOV_CFLAGS)							\
	$(GMODULE_CFLAGS) 						\
	$(DBUS_CFLAGS)

#modules_LTLIBRARIES = 							\
#	libwriteback-dummy.la

# dummy
#libwriteback_dummy_la_SOURCES = tracker-writeback-dummy.c
#libwriteback_dummy_la_LDFLAGS = $(module_flags)
#libwriteback_dummy_la_LIBADD = $(GLIB2_LIBS) $(GCOV_LIBS)

#
# Binaries
#
libexec_PROGRAMS = tracker-writeback

tracker_writeback_SOURCES = 						\
	$(dbus_sources)							\
	tracker-writeback-dispatcher.c					\
	tracker-writeback-dispatcher.h					\
	tracker-writeback-module.c					\
	tracker-writeback-module.h					\
	tracker-writeback.c						\
	tracker-writeback.h						\
	tracker-main.c

tracker_writeback_LDADD = 						\
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(DBUS_LIBS)							\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GCOV_LIBS)							\
	$(GLIB2_LIBS)

dbus_sources = 								\
	tracker-writeback-glue.h

%-glue.h: $(top_srcdir)/data/dbus/%.xml
	$(AM_V_GEN)$(DBUSBINDINGTOOL) --mode=glib-server --output=$@ --prefix=$(subst -,_,$*) $^

BUILT_SOURCES = 							\
	$(dbus_sources)

CLEANFILES = $(BUILT_SOURCES)
