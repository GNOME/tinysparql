include $(top_srcdir)/Makefile.decl

INCLUDES =							\
	-DSHAREDIR=\""$(datadir)"\"				\
	-DG_LOG_DOMAIN=\"Tracker\"				\
	-DTRACKER_COMPILATION					\
	-I$(top_srcdir)/src					\
	-I$(top_builddir)/src					\
	-I$(top_builddir)/src/libtracker-client			\
	$(WARN_CFLAGS)						\
	$(GLIB2_CFLAGS)						\
	$(GCOV_CFLAGS)						\
	$(GIO_CFLAGS)						\
	$(DBUS_CFLAGS)

noinst_PROGRAMS = tracker-miner-test

tracker_miner_test_SOURCES =   					\
	tracker-miner-test.c    				\
	tracker-miner-test.h    				\
	tracker-main.c

tracker_miner_test_LDADD =                             		\
       $(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la \
       $(top_builddir)/src/libtracker-client/libtracker-client-@TRACKER_API_VERSION@.la \
       $(top_builddir)/src/libtracker-common/libtracker-common.la \
       $(DBUS_LIBS)                                    		\
       $(GMODULE_LIBS)                                 		\
       $(GTHREAD_LIBS)                                 		\
       $(GIO_LIBS)                                     		\
       $(GCOV_LIBS)                                    		\
       $(GLIB2_LIBS)                                   		\
       -lz                                             		\
       -lm
