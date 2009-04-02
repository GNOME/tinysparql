include $(top_srcdir)/Makefile.decl

INCLUDES =								\
	-I$(top_srcdir)/src						\
	-DGETTEXT_PACKAGE="\"$(GETTEXT_PACKAGE)\""			\
	-DLOCALEDIR="\"$(localedir)\""					\
	-DTRACKER_COMPILATION						\
	$(WARN_CFLAGS)							\
	$(DBUS_CFLAGS)							\
	$(GMIME_CFLAGS)							\
	$(EVOLUTION_PLUGIN_CFLAGS)					\
	$(GCOV_CFLAGS)

%.eplug.in: %.eplug.xml
	LC_ALL=C $(INTLTOOL_MERGE) -x -u /tmp $< $@

%.eplug: %.eplug.in
	sed -e "s|+PLUGIN_INSTALL_DIR+|$(EVOLUTION_PLUGIN_INSTALL_DIR)|"	\
	    -e "s|+GETTEXT_PACKAGE+|$(GETTEXT_PACKAGE)|"			\
	    -e "s|+LOCALEDIR+|$(localedir)|"					\
	    $< > $@

eplugindir = $(EVOLUTION_PLUGIN_INSTALL_DIR)

eplugin_DATA = org-freedesktop-Tracker-evolution-plugin.eplug

eplugin_LTLIBRARIES = liborg-freedesktop-Tracker-evolution-plugin.la

module_flags = -module -avoid-version -no-undefined

pushd_modulesdir = $(libdir)/tracker/push-modules/daemon
pushi_modulesdir = $(libdir)/tracker/push-modules/indexer

pushd_modules_LTLIBRARIES = libtracker-module-evolution-daemon-module.la
pushi_modules_LTLIBRARIES = libtracker-module-evolution-indexer-module.la

libtracker_module_evolution_indexer_module_la_SOURCES =			\
	tracker-evolution-indexer.c					\
	tracker-evolution-indexer.h					\
	tracker-evolution-registrar-glue.h				\
	tracker-evolution-common.h

libtracker_module_evolution_daemon_module_la_SOURCES =	 		\
	tracker-evolution-registrar.c					\
	tracker-evolution-registrar.h					\
	tracker-evolution-common.h					\
	tracker-evolution-registrar-glue.h

libtracker_module_evolution_daemon_module_la_LDFLAGS = $(module_flags)

libtracker_module_evolution_daemon_module_la_LIBADD = 			\
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

libtracker_module_evolution_indexer_module_la_LDFLAGS = $(module_flags)

libtracker_module_evolution_indexer_module_la_LIBADD = 			\
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

liborg_freedesktop_Tracker_evolution_plugin_la_SOURCES = 		\
	tracker-evolution-plugin.c 			 		\
	tracker-evolution-plugin.h					\
	tracker-evolution-common.h					\
	tracker-evolution-plugin-glue.h

liborg_freedesktop_Tracker_evolution_plugin_la_LDFLAGS = -module -avoid-version
liborg_freedesktop_Tracker_evolution_plugin_la_LDLIBS = 		\
	$(EVOLUTION_PLUGIN_LIBS)					\
	$(DBUS_LIBS)							\
	$(GCOV_LIBS)

dbus_sources = 								\
	tracker-evolution-plugin-glue.h					\
	tracker-evolution-registrar-glue.h

%-glue.h: %.xml
	$(DBUSBINDINGTOOL) --mode=glib-server --output=$@ --prefix=$(subst -,_,$*) $^

BUILT_SOURCES = 							\
	$(dbus_sources)							\
	$(eplugin_DATA)

CLEANFILES = 								\
	$(BUILT_SOURCES) 						\
	org-freedesktop-Tracker-evolution-plugin.eplug

EXTRA_DIST = 								\
	org-freedesktop-Tracker-evolution-plugin.eplug.xml
