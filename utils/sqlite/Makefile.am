include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = query

INCLUDES = 						\
	$(WARN_CFLAGS)					\
	$(GIO_CFLAGS)					\
	$(GLIB2_CFLAGS)					\
	$(SQLITE3_CFLAGS)

query_SOURCES =						\
	query.c

query_LDADD =						\
	$(GIO_LIBS)					\
	$(GLIB2_LIBS)					\
	$(SQLITE3_LIBS)

