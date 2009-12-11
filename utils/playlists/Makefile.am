include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS =

if HAVE_TOTEM_PL_PARSER
noinst_PROGRAMS += playlist2ttl
endif

INCLUDES = 				\
	-DG_LOG_DOMAIN=\"Tracker\"	\
	$(WARN_CFLAGS)			\
	$(GLIB2_CFLAGS)			\
	$(GIO_CFLAGS)			\
	$(TOTEM_PL_PARSER_CFLAGS)

playlist2ttl_SOURCES = 			\
	playlist2ttl.c

playlist2ttl_LDADD = 			\
	$(TOTEM_PL_PARSER_LIBS) 	\
	$(GLIB2_LIBS)			\
	$(GIO_LIBS)			\
	$(GCOV_LIBS)