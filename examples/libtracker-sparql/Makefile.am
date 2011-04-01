include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = async-connection class-signal

AM_CPPFLAGS = $(BUILD_CFLAGS) \
	-I$(top_srcdir)/src \
	$(LIBTRACKER_SPARQL_CFLAGS)

LDADD = $(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-$(TRACKER_API_VERSION).la \
	$(BUILD_LIBS) \
	$(LIBTRACKER_SPARQL_LIBS)

async_connection_SOURCES = async-connection.c

class_signal_SOURCES = class-signal.c
