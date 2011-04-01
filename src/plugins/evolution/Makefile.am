include $(top_srcdir)/Makefile.decl

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	-DGETTEXT_PACKAGE="\"$(GETTEXT_PACKAGE)\""     \
	-DLOCALEDIR="\"$(localedir)\""                 \
	$(TRACKER_MINER_EVOLUTION_CFLAGS)

%.eplug.in: %.eplug.xml
	LC_ALL=C $(INTLTOOL_MERGE) -x -u /tmp $< $@

%.eplug: %.eplug.in
	sed -e "s|+PLUGIN_INSTALL_DIR+|$(TRACKER_MINER_EVOLUTION_INSTALL_DIR)|" \
	    -e "s|+GETTEXT_PACKAGE+|$(GETTEXT_PACKAGE)|" \
	    -e "s|+LOCALEDIR+|$(localedir)|"           \
	    $< > $@

eplugindir = $(TRACKER_MINER_EVOLUTION_INSTALL_DIR)

eplugin_DATA = org-freedesktop-Tracker-evolution-plugin.eplug

eplugin_LTLIBRARIES = liborg-freedesktop-Tracker-evolution-plugin.la

# module_flags = -module -avoid-version -no-undefined

liborg_freedesktop_Tracker_evolution_plugin_la_SOURCES = \
	tracker-evolution-plugin.c                     \
	tracker-evolution-plugin.h

liborg_freedesktop_Tracker_evolution_plugin_la_LDFLAGS = -module -avoid-version
liborg_freedesktop_Tracker_evolution_plugin_la_LIBADD = \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS)                                  \
	$(TRACKER_MINER_EVOLUTION_LIBS)

@INTLTOOL_DESKTOP_RULE@

BUILT_SOURCES =                                        \
	$(eplugin_DATA)

CLEANFILES =                                           \
	$(BUILT_SOURCES)                               \
	$(eplugin_DATA)

EXTRA_DIST =                                           \
	org-freedesktop-Tracker-evolution-plugin.eplug.xml
