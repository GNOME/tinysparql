include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = tracker-sql

INCLUDES = 								\
	-DTRACKER_COMPILATION						\
	-DTOP_SRCDIR=\"$(top_srcdir)\"					\
	-DTOP_BUILDDIR=\"$(top_builddir)\"				\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-I$(top_srcdir)/src						\
	$(WARN_CFLAGS)							\
	$(TRACKER_UTILS_CFLAGS)

tracker_sql_SOURCES =						\
	tracker-sql.c

tracker_sql_LDADD =						\
	$(top_builddir)/src/libtracker-data/libtracker-data.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(TRACKER_UTILS_LIBS)
