include $(top_srcdir)/Makefile.decl

INCLUDES =							\
	$(BUILD_CFLAGS)						\
	-DSHAREDIR=\""$(datadir)"\"				\
	-I$(top_srcdir)/src					\
	-I$(top_builddir)/src					\
	$(LIBTRACKER_MINER_CFLAGS)

noinst_PROGRAMS = tracker-miner-test

tracker_miner_test_SOURCES =   					\
	tracker-miner-test.c    				\
	tracker-miner-test.h    				\
	tracker-main.c

tracker_miner_test_LDADD =                             		\
       $(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la \
       $(top_builddir)/src/libtracker-client/libtracker-client-@TRACKER_API_VERSION@.la \
       $(top_builddir)/src/libtracker-sparql/libtracker-sparql-@TRACKER_API_VERSION@.la \
       $(top_builddir)/src/libtracker-common/libtracker-common.la \
       $(BUILD_LIBS)                                   		\
       $(LIBTRACKER_MINER_LIBS)
