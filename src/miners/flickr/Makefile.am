include $(top_srcdir)/Makefile.decl

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-DSHAREDIR=\""$(datadir)"\"                    \
	-DLOCALEDIR=\""$(localedir)"\"                 \
	-DLIBEXEC_PATH=\""$(libexecdir)"\"             \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	$(TRACKER_MINER_FLICKR_CFLAGS)

VALAFLAGS =                                            \
	--pkg gio-2.0                                  \
	--pkg rest-0.6                                 \
	--pkg posix                                    \
	--thread

libexec_PROGRAMS = tracker-miner-flickr

tracker_miner_flickr_VALASOURCES =                     \
	$(top_srcdir)/src/libtracker-sparql/tracker-sparql-@TRACKER_API_VERSION@.vapi \
	tracker-miner-flickr.vala

tracker_miner_flickr_SOURCES =                         \
	tracker-miner-flickr.vala.stamp                \
	$(tracker_miner_flickr_VALASOURCES:.vala=.c)   \
	tracker-writeback.c                            \
	tracker-writeback.h

tracker_miner_flickr_LDADD =                           \
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-sparql/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(BUILD_LIBS)                                  \
	$(TRACKER_MINER_FLICKR_LIBS)

vapi_sources =                                         \
	$(top_srcdir)/src/libtracker-miner/tracker-miner.vapi

tracker-miner-flickr.vala.stamp: $(tracker_miner_flickr_VALASOURCES) $(vapi_sources)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) $^
	$(AM_V_GEN)touch $@

BUILT_SOURCES =                                        \
	tracker-miner-flickr.vala.stamp

MAINTAINERCLEANFILES =                                 \
	$(tracker_miner_flickr_VALASOURCES:.vala=.c)   \
	tracker-miner-flickr.vala.stamp

EXTRA_DIST =                                           \
	$(tracker_miner_flickr_VALASOURCES)            \
	tracker-miner-flickr.vala.stamp
