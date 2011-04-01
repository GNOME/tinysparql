include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = tracker-resdump

AM_VALAFLAGS = \
	--pkg gio-2.0 \
	--pkg gee-1.0 \
	$(BUILD_VALAFLAGS) \
	$(top_srcdir)/src/libtracker-sparql/tracker-sparql-$(TRACKER_API_VERSION).vapi \
	$(top_srcdir)/src/libtracker-bus/tracker-bus.vapi

AM_CPPFLAGS =\
	$(BUILD_CFLAGS) \
	-I$(top_srcdir)/src \
	-I$(top_builddir)/src \
	$(LIBTRACKER_SPARQL_CFLAGS) \
	$(TRACKER_RESDUMP_CFLAGS)

LDADD =\
	$(top_builddir)/src/libtracker-data/libtracker-data.la \
	$(top_builddir)/src/libtracker-direct/libtracker-direct.la \
	$(top_builddir)/src/libtracker-bus/libtracker-bus.la \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-$(TRACKER_API_VERSION).la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS)\
	$(LIBTRACKER_SPARQL_LIBS) \
	$(TRACKER_RESDUMP_LIBS)

tracker_resdump_SOURCES = tracker-resdump.vala
tracker_resdump_CFLAGS = $(AM_CPPFLAGS)
tracker_resdump_VALAFLAGS = $(AM_VALAFLAGS)
tracker_resdump_LDADD = $(LDADD)

