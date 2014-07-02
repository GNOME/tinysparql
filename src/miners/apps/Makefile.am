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
