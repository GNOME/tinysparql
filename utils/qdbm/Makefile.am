include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = print search

INCLUDES = 								\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-I$(top_srcdir)/src						\
	$(QDBM_CFLAGS) 							\
	$(GLIB2_CFLAGS)

print_SOURCES = 							\
	print-words.c

print_LDADD = 								\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(top_builddir)/src/libtracker-db/libtracker-db.la	 	\
	$(QDBM_LIBS) 							\
	$(GLIB2_LIBS)							

search_SOURCES = 							\
	search-word.c

search_LDADD = 								\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(top_builddir)/src/libtracker-db/libtracker-db.la	 	\
	$(QDBM_LIBS) 							\
	$(GLIB2_LIBS)							

