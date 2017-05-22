AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	-DSHAREDIR=\""$(datadir)"\"                    \
	-DLOCALEDIR=\""$(localedir)"\"                 \
	-DLIBEXEC_PATH=\""$(libexecdir)"\"             \
	$(TRACKER_MINER_FS_CFLAGS)

libexec_PROGRAMS = tracker-miner-fs

# Prefer UPower for the power data if it's available, else fall back to HAL
if HAVE_UPOWER
power_headers =                                        \
	tracker-power.h
power_sources =                                        \
	tracker-power-upower.c
else
if HAVE_HAL
power_headers =                                        \
	tracker-power.h
power_sources =                                        \
	tracker-power-hal.c
endif
endif

tracker_miner_fs_SOURCES =                             \
	$(power_sources)                               \
	$(power_headers)                               \
	tracker-config.c                               \
	tracker-config.h                               \
	tracker-extract-watchdog.c                     \
	tracker-extract-watchdog.h                     \
	tracker-main.c                                 \
	tracker-miner-files.c                          \
	tracker-miner-files.h                          \
	tracker-miner-files-index.c                    \
	tracker-miner-files-index.h                    \
	tracker-miner-files-peer-listener.c            \
	tracker-miner-files-peer-listener.h            \
	tracker-storage.c                              \
	tracker-storage.h                              \
	tracker-writeback-listener.c                   \
	tracker-writeback-listener.h                   \
	tracker-writeback-dispatcher.c                 \
	tracker-writeback-dispatcher.h                 \
	tracker-writeback.c                            \
	tracker-writeback.h

tracker_miner_fs_LDADD =                               \
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-extract/libtracker-extract.la \
	$(top_builddir)/src/libtracker-data/libtracker-data.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS)                                  \
	$(TRACKER_MINER_FS_LIBS)

# DBus interfaces (XML files) and services
configdir = $(datadir)/tracker
config_DATA = tracker-miner-files-index.xml

%.service.in: %.service.in.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|" $< > $@

servicedir = $(DBUS_SERVICES_DIR)
service_DATA = org.freedesktop.Tracker1.Miner.Files.service

INTLTOOL_SERVICE_RULE='%.service:   %.service.in   $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< [$]@ -d -u'
@INTLTOOL_SERVICE_RULE@

tracker-miner-fs.service: tracker-miner-fs.service.in Makefile
	@sed -e "s|@libexecdir[@]|${libexecdir}|" $< > $@

systemd_userdir = $(prefix)/lib/systemd/user
systemd_user_DATA = tracker-miner-fs.service

install-data-hook:
	$(MKDIR_P) "$(DESTDIR)$(datadir)/tracker/miners" && \
	$(LN_S) -f "$(servicedir)/$(service_DATA)" \
                   "$(DESTDIR)$(datadir)/tracker/miners/$(service_DATA)"

uninstall-local:
	$(RM) "$(DESTDIR)$(datadir)/tracker/miners/$(service_DATA)" && \
	$(RM) "$(servicedir)/$(service_DATA)"

# Desktop files: First we fix the VERSION and libexecdir
if HAVE_LIBMEDIAART
requires="libmediaart-1.0"
else
requires=
endif

%.desktop.in: %.desktop.in.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|" \
	     -e "s|@VERSION[@]|${VERSION}|" $< > $@

autostartdir = $(sysconfdir)/xdg/autostart
autostart_DATA = tracker-miner-fs.desktop

@INTLTOOL_DESKTOP_RULE@

# Configuration / GSettings
gsettings_ENUM_NAMESPACE = org.freedesktop.Tracker
gsettings_ENUM_FILES = $(top_srcdir)/src/libtracker-common/tracker-enums.h

gsettings_SCHEMAS = org.freedesktop.Tracker.Miner.Files.gschema.xml

@INTLTOOL_XML_NOMERGE_RULE@

@GSETTINGS_RULES@

# Dist / Clean rules for all data
CLEANFILES = \
	$(autostart_DATA) \
	$(autostart_DATA:.desktop=.desktop.in) \
	$(service_DATA) \
	$(service_DATA:.service=.service.in) \
	$(systemd_user_DATA) \
	$(gsettings_SCHEMAS)

EXTRA_DIST = \
	$(autostart_DATA:.desktop=.desktop.in.in) \
	$(config_DATA) \
	$(service_DATA:.service=.service.in.in) \
	$(systemd_user_DATA:.service=.service.in) \
	$(gsettings_SCHEMAS:.xml=.xml.in) \
	meson.build
