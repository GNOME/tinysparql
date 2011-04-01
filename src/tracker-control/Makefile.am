include $(top_srcdir)/Makefile.decl

AM_CPPFLAGS = \
	$(BUILD_CFLAGS) \
	-DLOCALEDIR=\""$(localedir)"\" \
	-I$(top_srcdir)/src \
	-I$(top_builddir)/src \
	$(TRACKER_CONTROL_CFLAGS)

bin_PROGRAMS = tracker-control

tracker_control_SOURCES = \
	tracker-control.h \
	tracker-control-general.c \
	tracker-control-miners.c \
	tracker-control-status.c \
	tracker-control.c

tracker_control_LDADD = \
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-data/libtracker-data.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_CONTROL_LIBS)
