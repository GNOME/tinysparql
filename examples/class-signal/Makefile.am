include $(top_srcdir)/Makefile.decl

INCLUDES = \
	$(BUILD_CFLAGS) 		\
	-I$(top_srcdir)/src 		\
	$(LIBTRACKER_SPARQL_CFLAGS)

noinst_PROGRAMS = class-signal

class_signal_SOURCES = class-signal.c

class_signal_LDADD = 			\
	$(top_builddir)/src/libtracker-sparql/libtracker-sparql-$(TRACKER_API_VERSION).la \
	$(BUILD_LIBS) 			\
	$(LIBTRACKER_SPARQL_LIBS)
