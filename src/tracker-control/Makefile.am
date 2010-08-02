include $(top_srcdir)/Makefile.decl

INCLUDES =								\
	-DLOCALEDIR=\""$(localedir)"\" 					\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	-I$(top_builddir)/src						\
	-I$(top_builddir)/src/libtracker-client				\
	-I$(top_srcdir)/src/libtracker-data				\
	-I$(top_builddir)/src/libtracker-data				\
	$(WARN_CFLAGS)							\
	$(GCOV_CFLAGS)							\
	$(DBUS_CFLAGS)							\
	$(GIO_CFLAGS)							\
	$(GTHREAD_CFLAGS)						\
	$(GLIB2_CFLAGS)

libs = 									\
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la	\
	$(top_builddir)/src/libtracker-client/libtracker-client-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-data/libtracker-data.la		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(DBUS_LIBS)							\
	$(GIO_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GLIB2_LIBS)							\
	$(GCOV_LIBS)

bin_PROGRAMS = tracker-control

tracker_control_SOURCES = 						\
	$(dbus_sources)							\
	tracker-control.c

tracker_control_LDADD = $(libs)

dbus_sources = 								\
	tracker-miner-files-reindex-client.h

%-client.h: $(top_srcdir)/data/dbus/%.xml
	$(AM_V_GEN)$(DBUSBINDINGTOOL) --mode=glib-client --output=$@ --prefix=$(subst -,_,$*) $^

BUILT_SOURCES = 							\
	$(dbus_sources)

CLEANFILES = $(BUILT_SOURCES)
