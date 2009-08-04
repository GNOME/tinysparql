include $(top_srcdir)/Makefile.decl

INCLUDES =						\
	-DSHAREDIR=\""$(datadir)"\"			\
	-DG_LOG_DOMAIN=\"Tracker\"			\
	-DTRACKER_COMPILATION				\
	-DI_KNOW_THE_DEVICEKIT_POWER_API_IS_SUBJECT_TO_CHANGE	\
	-I$(top_srcdir)/src				\
	$(WARN_CFLAGS)					\
	$(GLIB2_CFLAGS)					\
	$(GCOV_CFLAGS)					\
	$(GDKPIXBUF_CFLAGS)				\
	$(GIO_CFLAGS)					\
	$(PANGO_CFLAGS)					\
	$(HAL_CFLAGS)					\
	$(DEVKIT_POWER_CFLAGS)				\
	$(DBUS_CFLAGS)					\
	$(UNAC_CFLAGS)

libtracker_minerdir = $(libdir)/tracker-$(TRACKER_API_VERSION)
libtracker_minerincludedir=$(includedir)/tracker-$(TRACKER_API_VERSION)/libtracker-miner/
libtracker_miner_LTLIBRARIES = libtracker-miner.la

# tracker-miner-crawler.c				
# tracker-miner-crawler.h				

libtracker_miner_la_SOURCES = 				\
	tracker-crawler.c				\
	tracker-crawler.h				\
	tracker-dbus.c					\
	tracker-dbus.h					\
	tracker-marshal.c				\
	tracker-marshal.h				\
	tracker-miner-dbus.h				\
	tracker-miner.c					\
	tracker-miner.h					\
	tracker-monitor.c				\
	tracker-monitor.h				\
	tracker-processor.c				\
	tracker-processor.h				\
	tracker-utils.c					\
	tracker-utils.h

libtracker_minerinclude_HEADERS = 			\
	tracker-miner.h 				\
	tracker-processor.h

libtracker_miner_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

libtracker_miner_la_LIBADD = 				\
	$(top_builddir)/src/libstemmer/libstemmer.la	\
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(top_builddir)/src/libtracker/libtrackerclient-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libinotify/libinotify.la	\
	$(HAL_LIBS)					\
	$(DEVKIT_POWER_LIBS)				\
	$(DBUS_LIBS)					\
	$(UNAC_LIBS)					\
	$(PANGO_LIBS)					\
	$(GIO_LIBS)					\
	$(GCOV_LIBS)					\
	$(GLIB2_LIBS)					\
	$(GDKPIXBUF_LIBS)

tracker-marshal.h: tracker-marshal.list
	$(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --header > $@

tracker-marshal.c: tracker-marshal.list
	(echo "#include \"tracker-marshal.h\""; \
	 $(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --body) > $@

marshal_sources = 					\
	tracker-marshal.c				\
	tracker-marshal.h

dbus_sources = tracker-miner-glue.h

%-glue.h: $(top_srcdir)/data/dbus/%.xml
	$(DBUSBINDINGTOOL) --mode=glib-server --output=$@ --prefix=$(subst -,_,$*) $^

BUILT_SOURCES = $(dbus_sources) $(marshal_sources)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = tracker-marshal.list

libexec_PROGRAMS = tracker-miner-test

tracker_miner_test_SOURCES = 	\
	tracker-miner-test.c 	\
	tracker-miner-test.h 	\
	tracker-main.c

tracker_miner_test_LDADD = 				\
	libtracker-miner.la			 	\
	$(DBUS_LIBS)					\
	$(GMODULE_LIBS)					\
	$(GTHREAD_LIBS)					\
	$(GIO_LIBS)					\
	$(GCOV_LIBS)					\
	$(GLIB2_LIBS)					\
	$(RAPTOR_LIBS)					\
	-lz						\
	-lm
