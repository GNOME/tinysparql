include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = tracker-miner-test

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	-DSHAREDIR=\""$(datadir)"\"                    \
	$(LIBTRACKER_MINER_CFLAGS)

LDADD =                                                \
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_MINER_LIBS)

tracker_miner_test_SOURCES =                           \
	tracker-miner-test.c                           \
	tracker-miner-test.h                           \
	tracker-main.c
