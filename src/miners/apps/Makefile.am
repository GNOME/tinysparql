AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	-DSHAREDIR=\""$(datadir)"\"                    \
	-DLOCALEDIR=\""$(localedir)"\"                 \
	-DLIBEXEC_PATH=\""$(libexecdir)"\"             \
	$(TRACKER_MINER_APPS_CFLAGS)

libexec_PROGRAMS = tracker-miner-apps

tracker_miner_apps_SOURCES =                           \
	tracker-main.c                                 \
	tracker-miner-applications.c                   \
	tracker-miner-applications.h

tracker_miner_apps_LDADD =                             \
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS)                                  \
	$(TRACKER_MINER_APPS_LIBS)

# DBus interfaces (XML files) and services
service_in_files = org.freedesktop.Tracker1.Miner.Applications.service.in

%.service: %.service.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|" $< > $@

servicedir = $(DBUS_SERVICES_DIR)
service_DATA = org.freedesktop.Tracker1.Miner.Applications.service

# Desktop files: First we fix the VERSION and libexecdir
tracker-miner-apps.desktop.in: tracker-miner-apps.desktop.in.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|" \
	     -e "s|@VERSION[@]|${VERSION}|" $< > $@

desktop_in_in_files = tracker-miner-apps.desktop.in.in
desktop_in_files = tracker-miner-apps.desktop.in

autostartdir = $(sysconfdir)/xdg/autostart
autostart_DATA = tracker-miner-apps.desktop

@INTLTOOL_DESKTOP_RULE@

CLEANFILES = $(autostart_DATA) $(desktop_in_files) $(service_DATA)
EXTRA_DIST = $(desktop_in_in_files) $(service_in_files)
