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

noinst_PROGRAMS = 						\
	steroids-sparql						\
	steroids-sparql-async					\
	steroids-sparql-update-async				\
	benchmark						\
	benchmark-update

steroids_sparql_SOURCES =					\
	steroids-sparql.c

steroids_sparql_update_async_SOURCES = steroids-sparql-update-async.c

benchmark_SOURCES = benchmark.c

benchmark_update_SOURCES = benchmark-update.c

steroids_sparql_LDADD =						\
       $(top_builddir)/src/libtracker-client/libtracker-client-@TRACKER_API_VERSION@.la \
       $(top_builddir)/src/libtracker-common/libtracker-common.la \
       $(DBUS_LIBS)						\
       $(GMODULE_LIBS)						\
       $(GTHREAD_LIBS)						\
       $(GIO_LIBS)						\
       $(GCOV_LIBS)						\
       $(GLIB2_LIBS)						\
       -lz							\
       -lm

steroids_sparql_async_LDADD = $(steroids_sparql_LDADD)

steroids_sparql_update_async_LDADD = $(steroids_sparql_LDADD)

benchmark_LDADD = $(steroids_sparql_LDADD)

benchmark_update_LDADD = $(steroids_sparql_LDADD)
