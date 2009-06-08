include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = ontology-validator ttl2html data-validator

INCLUDES = 				\
	-DG_LOG_DOMAIN=\"Tracker\"	\
	$(WARN_CFLAGS)			\
	$(GLIB2_CFLAGS)			\
	$(RAPTOR_CFLAGS)		\
	$(GIO_CFLAGS)

ontology_validator_SOURCES =		\
	ontology-validator.c

ontology_validator_LDADD =		\
	$(RAPTOR_LIBS) 			\
	$(GLIB2_LIBS)			\
	$(GIO_LIBS)							

data_validator_SOURCES = 		\
	data-validator.c

data_validator_LDADD =			\
	$(RAPTOR_LIBS) 			\
	$(GLIB2_LIBS)			\
	$(GIO_LIBS)							

ttl2html_SOURCES =			\
	ttl2html.c			\
	ttl_loader.h			\
	ttl_loader.c			\
	ttl_model.h 			\
	ttl_model.c			\
	ttl_html.h			\
	ttl_html.c

ttl2html_LDADD = 			\
	$(RAPTOR_LIBS) 			\
	$(GLIB2_LIBS)			\
	$(GIO_LIBS)	
