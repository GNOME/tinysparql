include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS += $(test_programs)

test_programs = gvdb-test

AM_CPPFLAGS =                                          \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	-DSHAREDIR=\""$(datadir)"\"                    \
	$(LIBTRACKER_DATA_CFLAGS)

gvdb_test_SOURCES = gvdb-test.c
gvdb_test_LDADD = 	$(top_builddir)/src/gvdb/libgvdb.la 	\
			$(BUILD_LIBS) 				\
			$(GVDB_LIBS) 				\
			$(TRACKER_UTILS_LIBS)

gvdb_test_CFLAGS = 	$(BUILD_CFLAGS) \
			$(GVDB_CFLAGS) 	\
			$(TRACKER_UTILS_CFLAGS)
