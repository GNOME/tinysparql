include $(top_srcdir)/Makefile.decl

AM_CPPFLAGS =								\
	$(BUILD_CFLAGS)							\
	-DLOCALEDIR=\""$(localedir)"\" 					\
	-I$(top_srcdir)/src						\
	-I$(top_builddir)/src						\
	$(TRACKER_CONTROL_CFLAGS)

bin_PROGRAMS = tracker-control

tracker_control_SOURCES = 						\
	$(dbus_sources)							\
	tracker-control.c

tracker_control_LDADD =							\
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la	\
	$(top_builddir)/src/libtracker-sparql/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-data/libtracker-data.la		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(BUILD_LIBS)							\
	$(TRACKER_CONTROL_LIBS)

dbus_sources = 								\
	tracker-miner-files-index-client.h

%-client.h: $(top_srcdir)/data/dbus/%.xml
	$(AM_V_GEN)$(DBUSBINDINGTOOL) --mode=glib-client --output=$@ --prefix=$(subst -,_,$*) $^

BUILT_SOURCES = 							\
	$(dbus_sources)

CLEANFILES = $(BUILT_SOURCES)
