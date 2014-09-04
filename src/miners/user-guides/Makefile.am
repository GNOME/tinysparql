AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	-DSHAREDIR=\""$(datadir)"\"                    \
	-DLOCALEDIR=\""$(localedir)"\"                 \
	-DLIBEXEC_PATH=\""$(libexecdir)"\"             \
	$(TRACKER_MINER_USER_GUIDES_CFLAGS)

libexec_PROGRAMS = tracker-miner-user-guides

tracker_miner_user_guides_SOURCES =                    \
	tracker-main.c                                 \
	tracker-miner-user-guides.c                    \
	tracker-miner-user-guides.h

tracker_miner_user_guides_LDADD =                      \
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS)                                  \
	$(TRACKER_MINER_USER_GUIDES_LIBS)

# Desktop files: First we fix the VERSION and libexecdir
tracker-miner-user-guides.desktop.in: tracker-miner-user-guides.desktop.in.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|" \
	     -e "s|@VERSION[@]|${VERSION}|" $< > $@

desktop_in_in_files = tracker-miner-user-guides.desktop.in.in
desktop_in_files = tracker-miner-user-guides.desktop.in

autostartdir = $(sysconfdir)/xdg/autostart
autostart_DATA = tracker-miner-user-guides.desktop

@INTLTOOL_DESKTOP_RULE@

CLEANFILES = $(autostart_DATA) $(desktop_in_files)
EXTRA_DIST = $(desktop_in_in_files)
