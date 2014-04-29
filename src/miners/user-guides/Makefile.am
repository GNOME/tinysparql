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
