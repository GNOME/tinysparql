include $(top_srcdir)/Makefile.decl

INCLUDES = -I$(top_srcdir)/src						\
	-DGETTEXT_PACKAGE="\"$(GETTEXT_PACKAGE)\""			\
	-DLOCALEDIR="\"$(localedir)\""					\
	-DTRACKER_COMPILATION						\
	$(WARN_CFLAGS)							\
	$(DBUS_CFLAGS)							\
	$(GMIME_CFLAGS)							\
	$(GCOV_CFLAGS)

module_flags = -module -avoid-version -no-undefined

pushd_modulesdir = $(libdir)/tracker/push-modules/daemon
pushi_modulesdir = $(libdir)/tracker/push-modules/indexer

pushd_modules_LTLIBRARIES = libtracker-module-rss-daemon-module.la
pushi_modules_LTLIBRARIES = libtracker-module-rss-indexer-module.la

libtracker_module_rss_indexer_module_la_SOURCES =			\
	tracker-rss-indexer.c						\
	tracker-rss-indexer.h						\
	tracker-rss-registrar-glue.h					\
	tracker-rss-common.h

libtracker_module_rss_daemon_module_la_SOURCES =			\
	tracker-rss-registrar.c						\
	tracker-rss-registrar.h						\
	tracker-rss-common.h						\
	tracker-rss-registrar-glue.h

libtracker_module_rss_daemon_module_la_LDFLAGS = $(module_flags)

libtracker_module_rss_daemon_module_la_LIBADD = 			\
	$(top_builddir)/src/libtracker-data/libtracker-data.la          \
	$(top_builddir)/src/libtracker-db/libtracker-db.la              \
	$(top_builddir)/src/libtracker-common/libtracker-common.la      \
	$(GMODULE_LIBS)							\
	$(DBUS_LIBS)                                                    \
	$(GTHREAD_LIBS)                                                 \
	$(GIO_LIBS)                                                     \
	$(GLIB2_LIBS)                                                   \
	$(RAPTOR_LIBS)							\
	$(GCOV_LIBS)

libtracker_module_rss_indexer_module_la_LDFLAGS = $(module_flags)

libtracker_module_rss_indexer_module_la_LIBADD = 			\
	$(top_builddir)/src/libtracker-data/libtracker-data.la          \
	$(top_builddir)/src/libtracker-db/libtracker-db.la              \
	$(top_builddir)/src/libtracker-common/libtracker-common.la      \
	$(GMODULE_LIBS)							\
	$(DBUS_LIBS)                                                    \
	$(GTHREAD_LIBS)                                                 \
	$(GIO_LIBS)                                                     \
	$(GLIB2_LIBS)                                                   \
	$(RAPTOR_LIBS)							\
	$(GMIME_LIBS)							\
	$(GCOV_LIBS)

dbus_sources = tracker-rss-registrar-glue.h

%-glue.h: %.xml
	$(DBUSBINDINGTOOL) --mode=glib-server --output=$@ --prefix=$(subst -,_,$*) $^

BUILT_SOURCES = $(dbus_sources)

CLEANFILES = $(BUILT_SOURCES)

