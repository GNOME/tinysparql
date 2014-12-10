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

BUILT_SOURCES = $(eplugin_DATA)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(eplugin_DATA)

EXTRA_DIST = org-freedesktop-Tracker-evolution-plugin.eplug.xml

# DBus interfaces (XML files) and services
%.service.in: %.service.in.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|" $< > $@

servicedir = $(DBUS_SERVICES_DIR)
service_DATA = org.freedesktop.Tracker1.Miner.EMails.service

INTLTOOL_SERVICE_RULE='%.service:   %.service.in   $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< [$]@ -d -u'
@INTLTOOL_SERVICE_RULE@

install-data-hook:
	$(MKDIR_P) "$(DESTDIR)$(datadir)/tracker/miners" && \
	$(LN_S) -f "$(servicedir)/$(service_DATA)" \
                   "$(DESTDIR)$(datadir)/tracker/miners/$(service_DATA)"

uninstall-local:
	$(RM) "$(DESTDIR)$(datadir)/tracker/miners/$(service_DATA)" && \
	$(RM) "$(servicedir)/$(service_DATA)"

# Dist / Clean rules for all data
CLEANFILES += \
	$(autostart_DATA) \
	$(desktop_in_files) \
	$(service_DATA) \
	$(service_DATA:.service=.service.in)

EXTRA_DIST += \
	$(desktop_in_in_files) \
	$(service_DATA:.service=.service.in.in)
