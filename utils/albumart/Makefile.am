include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = albumart-strip

INCLUDES = 								\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	$(WARN_CFLAGS)							\
	$(GLIB2_CFLAGS)

albumart_strip_SOURCES =						\
	albumart-strip.c

albumart_strip_LDADD =							\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(GLIB2_LIBS)

