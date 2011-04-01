include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = tracker-sql

INCLUDES =                                             \
	$(BUILD_CFLAGS)                                \
	-DLOCALEDIR=\""$(localedir)"\"                 \
	-I$(top_srcdir)/src                            \
	$(TRACKER_UTILS_CFLAGS)

LDADD =                                                \
	$(top_builddir)/src/libtracker-data/libtracker-data.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(TRACKER_UTILS_LIBS)

tracker_sql_SOURCES = tracker-sql.c
