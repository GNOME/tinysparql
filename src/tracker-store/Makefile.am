include $(top_srcdir)/Makefile.decl

INCLUDES =								\
	-DSHAREDIR=\""$(datadir)"\"					\
	-DLOCALEDIR=\""$(localedir)"\" 					\
	-DMAIL_MODULES_DIR=\""$(libdir)"/tracker-$(TRACKER_API_VERSION)/mail-modules\"         \
	-DPUSH_MODULES_DIR=\""$(libdir)/tracker-$(TRACKER_API_VERSION)/push-modules/daemon"\"	\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	-I$(top_srcdir)/src/libtracker-sparql				\
	-I$(top_srcdir)/src/libtracker-data				\
	-I$(top_builddir)/src						\
	-I$(top_builddir)/src/libtracker-sparql				\
	-I$(top_builddir)/src/libtracker-data				\
	$(WARN_CFLAGS)							\
	$(GCOV_CFLAGS)							\
	$(TRACKER_STORE_CFLAGS)

#
# Daemon sources
#
libexec_PROGRAMS = tracker-store

tracker_store_SOURCES =							\
	$(marshal_sources)						\
	$(dbus_sources)							\
	tracker-backup.c						\
	tracker-backup.h						\
	tracker-config.c						\
	tracker-config.h						\
	tracker-dbus.c							\
	tracker-dbus.h							\
	tracker-events.c						\
	tracker-events.h						\
	tracker-writeback.c						\
	tracker-writeback.h						\
	tracker-main.c							\
	tracker-resources.c						\
	tracker-resources.h						\
	tracker-statistics.c						\
	tracker-statistics.h						\
	tracker-store.c							\
	tracker-store.h							\
	tracker-status.c						\
	tracker-status.h						\
	tracker-steroids.c						\
	tracker-steroids.h

tracker_store_LDADD =							\
	$(plugin_libs)							\
	$(top_builddir)/src/libtracker-data/libtracker-data.la		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(top_builddir)/src/libtracker-sparql/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(GCOV_LIBS)							\
	$(TRACKER_STORE_LIBS)

marshal_sources =                                         		\
        tracker-marshal.h                             			\
        tracker-marshal.c

dbus_sources = 								\
	tracker-backup-glue.h						\
	tracker-resources-glue.h					\
	tracker-statistics-glue.h					\
	tracker-status-glue.h

tracker-marshal.h: tracker-marshal.list
	$(AM_V_GEN)$(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --header > $@

tracker-marshal.c: tracker-marshal.list
	$(AM_V_GEN)echo "#include \"tracker-marshal.h\"" > $@ &&	\
		   $(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --body >> $@

%-glue.h: $(top_srcdir)/data/dbus/%.xml
	$(AM_V_GEN)$(DBUSBINDINGTOOL) --mode=glib-server --output=$@ --prefix=$(subst -,_,$*) $^

%-client.h: $(top_srcdir)/data/dbus/%.xml
	$(AM_V_GEN)$(DBUSBINDINGTOOL) --mode=glib-client --output=$@ --prefix=$(subst -,_,$*) $^

BUILT_SOURCES = 							\
	$(marshal_sources)						\
	$(dbus_sources)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = tracker-marshal.list

