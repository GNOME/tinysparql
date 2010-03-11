include $(top_srcdir)/Makefile.decl

INCLUDES =						\
	-DSHAREDIR=\""$(datadir)"\"			\
	-DDBUS_SERVICES_DIR=\""$(DBUS_SERVICES_DIR)"\"	\
	-DG_LOG_DOMAIN=\"Tracker\"			\
	-DTRACKER_COMPILATION				\
	-DI_KNOW_THE_DEVICEKIT_POWER_API_IS_SUBJECT_TO_CHANGE	\
	-DTRACKER_MINERS_DIR=\""$(datadir)/tracker/miners"\" 	\
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

lib_LTLIBRARIES = libtracker-miner-@TRACKER_API_VERSION@.la

libtracker_minerdir = $(libdir)/tracker-$(TRACKER_API_VERSION)
libtracker_minerincludedir=$(includedir)/tracker-$(TRACKER_API_VERSION)/libtracker-miner/

libtracker_miner_@TRACKER_API_VERSION@_la_SOURCES = 	\
	tracker-crawler.c				\
	tracker-crawler.h				\
	tracker-dbus.c					\
	tracker-dbus.h					\
	tracker-marshal.c				\
	tracker-marshal.h				\
	tracker-miner.c					\
	tracker-miner.h					\
	tracker-miner-dbus.h				\
	tracker-miner-fs.c				\
	tracker-miner-fs.h				\
	tracker-miner-manager.c				\
	tracker-miner-manager.h				\
	tracker-monitor.c				\
	tracker-monitor.h				\
	tracker-storage.c				\
	tracker-storage.h				\
	tracker-utils.c					\
	tracker-utils.h					\
	tracker-thumbnailer.c

libtracker_minerinclude_HEADERS = 			\
	tracker-crawler.h				\
	tracker-miner.h 				\
	tracker-miner-dbus.h 				\
	tracker-miner-fs.h				\
	tracker-miner-manager.h				\
	tracker-storage.h				\
	tracker-thumbnailer.h

libtracker_miner_@TRACKER_API_VERSION@_la_LDFLAGS = 	\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-symbols-regex '^tracker_(miner|thumbnailer|crawler|storage)_.*'

libtracker_miner_@TRACKER_API_VERSION@_la_LIBADD = 	\
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(top_builddir)/src/libtracker-client/libtracker-client-@TRACKER_API_VERSION@.la \
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
	$(AM_V_GEN)$(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --header > $@

tracker-marshal.c: tracker-marshal.list
	$(AM_V_GEN)(echo "#include \"tracker-marshal.h\""; \
		    $(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --body) > $@

marshal_sources = 					\
	tracker-marshal.c				\
	tracker-marshal.h

dbus_sources = 						\
	tracker-miner-glue.h				\
	tracker-miner-client.h

%-glue.h: $(top_srcdir)/data/dbus/%.xml
	$(AM_V_GEN)$(DBUSBINDINGTOOL) --mode=glib-server --output=$@ --prefix=$(subst -,_,$*) $^

%-client.h: $(top_srcdir)/data/dbus/%.xml
	$(AM_V_GEN)$(DBUSBINDINGTOOL) --mode=glib-client --output=$@ --prefix=$(subst -,_,$*) $^

BUILT_SOURCES = $(dbus_sources) $(marshal_sources)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = tracker-marshal.list
