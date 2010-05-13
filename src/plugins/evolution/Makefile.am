include $(top_srcdir)/Makefile.decl

INCLUDES =								\
	-I$(top_srcdir)/src						\
	-DGETTEXT_PACKAGE="\"$(GETTEXT_PACKAGE)\""			\
	-DLOCALEDIR="\"$(localedir)\""					\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTRACKER_COMPILATION						\
	-I$(top_builddir)/src						\
	-I$(top_builddir)/src/libtracker-client				\
	$(WARN_CFLAGS)							\
	$(DBUS_CFLAGS)							\
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

liborg_freedesktop_Tracker_evolution_plugin_la_SOURCES = 		\
	tracker-evolution-plugin.c 			 		\
	tracker-evolution-plugin.h

liborg_freedesktop_Tracker_evolution_plugin_la_LDFLAGS = -module -avoid-version
liborg_freedesktop_Tracker_evolution_plugin_la_LIBADD = 		\
	$(top_builddir)/src/libtracker-client/libtracker-client-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la	\
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(EVOLUTION_PLUGIN_LIBS)					\
	$(DBUS_LIBS)							\
	$(GCOV_LIBS)

@INTLTOOL_DESKTOP_RULE@

%-glue.h: %.xml
	$(DBUSBINDINGTOOL) --mode=glib-server --output=$@ --prefix=$(subst -,_,$*) $^

BUILT_SOURCES = 							\
	$(eplugin_DATA)

CLEANFILES = 								\
	$(BUILT_SOURCES) 						\
	$(eplugin_DATA)

EXTRA_DIST =								\
	org-freedesktop-Tracker-evolution-plugin.eplug.xml
