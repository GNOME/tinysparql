include ../Makefile-plugins.am

noinst_PROGRAMS = dictionary-builder

CLEANFILES = da.c de.c en.c es.c it.c no.c pt.c peda.c pede.c peen.c pees.c peit.c peno.c pept.c

da.c: dictionary-builder$(EXEEXT)
	./dictionary-builder $(srcdir)/da > da.c
de.c: dictionary-builder$(EXEEXT)
	./dictionary-builder $(srcdir)/de > de.c
en.c: dictionary-builder$(EXEEXT)
	./dictionary-builder $(srcdir)/en > en.c
es.c: dictionary-builder$(EXEEXT)
	./dictionary-builder $(srcdir)/es > es.c
it.c: dictionary-builder$(EXEEXT)
	./dictionary-builder $(srcdir)/it > it.c
no.c: dictionary-builder$(EXEEXT)
	./dictionary-builder $(srcdir)/no > no.c
pt.c: dictionary-builder$(EXEEXT)
	./dictionary-builder $(srcdir)/pt > pt.c

peda.c:
	cat peXX.c | sed -e "s/XX/da/" > peda.c
pede.c:
	cat peXX.c | sed -e "s/XX/de/" > pede.c
peen.c:
	cat peXX.c | sed -e "s/XX/en/" > peen.c
pees.c:
	cat peXX.c | sed -e "s/XX/es/" > pees.c
peit.c:
	cat peXX.c | sed -e "s/XX/it/" > peit.c
peno.c:
	cat peXX.c | sed -e "s/XX/no/" > peno.c
pept.c:
	cat peXX.c | sed -e "s/XX/pt/" > pept.c

dictionary_builder_SOURCES = \
 dictionary-builder.c 
dictionary_builder_LDADD = \
 $(LIBINTL) 

plugin_LTLIBRARIES = \
 libextractor_printable_da.la \
 libextractor_printable_de.la \
 libextractor_printable_en.la \
 libextractor_printable_es.la \
 libextractor_printable_it.la \
 libextractor_printable_no.la \
 libextractor_printable_pt.la 


libextractor_printable_da_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la
libextractor_printable_de_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la
libextractor_printable_en_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la
libextractor_printable_es_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la
libextractor_printable_it_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la
libextractor_printable_no_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la
libextractor_printable_pt_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la


libextractor_printable_da_la_SOURCES = \
  da.c peda.c bloomfilter.h printableextractor.h bloomfilter-def.h
libextractor_printable_da_la_LDFLAGS = \
 $(PLUGINFLAGS) $(retaincommand)

libextractor_printable_de_la_SOURCES = \
  de.c pede.c bloomfilter.h printableextractor.h bloomfilter-def.h
libextractor_printable_de_la_LDFLAGS = \
 $(PLUGINFLAGS) $(retaincommand)

libextractor_printable_en_la_SOURCES = \
  en.c peen.c bloomfilter.h printableextractor.h bloomfilter-def.h
libextractor_printable_en_la_LDFLAGS = \
 $(PLUGINFLAGS) $(retaincommand)

libextractor_printable_es_la_SOURCES = \
  es.c pees.c bloomfilter.h printableextractor.h bloomfilter-def.h
libextractor_printable_es_la_LDFLAGS = \
 $(PLUGINFLAGS) $(retaincommand)

libextractor_printable_it_la_SOURCES = \
  it.c peit.c bloomfilter.h printableextractor.h bloomfilter-def.h
libextractor_printable_it_la_LDFLAGS = \
 $(PLUGINFLAGS) $(retaincommand)

libextractor_printable_no_la_SOURCES = \
  no.c peno.c bloomfilter.h printableextractor.h bloomfilter-def.h
libextractor_printable_no_la_LDFLAGS = \
 $(PLUGINFLAGS) $(retaincommand)

libextractor_printable_pt_la_SOURCES = \
  pt.c pept.c bloomfilter.h printableextractor.h bloomfilter-def.h
libextractor_printable_pt_la_LDFLAGS = \
 $(PLUGINFLAGS) $(retaincommand)
