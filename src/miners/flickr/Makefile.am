include $(top_srcdir)/Makefile.decl

libexec_PROGRAMS = tracker-miner-flickr

AM_VALAFLAGS =                                         \
	--thread                                       \
	--pkg gio-2.0 --pkg rest-0.6 --pkg posix       \
	$(BUILD_VALAFLAGS)                             \
	$(top_srcdir)/src/vapi/glib-2.0-fixes.vapi     \
	$(top_srcdir)/src/libtracker-sparql/tracker-sparql-@TRACKER_API_VERSION@.vapi \
	$(top_srcdir)/src/libtracker-miner/tracker-miner.vapi

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-DSHAREDIR=\""$(datadir)"\"                    \
	-DLOCALEDIR=\""$(localedir)"\"                 \
	-DLIBEXEC_PATH=\""$(libexecdir)"\"             \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	$(TRACKER_MINER_FLICKR_CFLAGS)

LDADD =                                                \
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(BUILD_LIBS)                                  \
	$(TRACKER_MINER_FLICKR_LIBS)

tracker_miner_flickr_SOURCES =                         \
	tracker-miner-flickr.vala                      \
	tracker-writeback.c

noinst_HEADERS =                                       \
	tracker-writeback.h
