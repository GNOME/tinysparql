include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = mp3-genre-leading-uppercase

INCLUDES = 								\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	$(WARN_CFLAGS)							\
	$(GLIB2_CFLAGS)

mp3_genre_leading_uppercase_SOURCES =					\
	mp3-genre-leading-uppercase.c

mp3_genre_leading_uppercase_LDADD =					\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(GLIB2_LIBS)

