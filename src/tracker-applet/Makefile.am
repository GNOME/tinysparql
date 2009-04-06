include $(top_srcdir)/Makefile.decl

INCLUDES =								\
	-DSHAREDIR=\""$(datadir)"\"					\
	-DLOCALEDIR=\""$(localedir)"\" 					\
	-DTRACKER_LIBEXECDIR=\""$(libexecdir)"\" 			\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	$(WARN_CFLAGS)							\
	$(LIBGLADE_CFLAGS)						\
	$(TRACKER_APPLET_CFLAGS)					\
	$(GCOV_CFLAGS)

bin_PROGRAMS = tracker-applet

tracker_applet_LDADD = 							\
	$(top_builddir)/src/libtracker/libtrackerclient.la 		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(TRACKER_APPLET_LIBS)						\
	$(LIBGLADE_LIBS)						\
	$(GCOV_LIBS)

tracker-marshal.h: tracker-marshal.list
	$(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --header > $@

tracker-marshal.c: tracker-marshal.list
	$(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --body > $@

marshal_sources =                                         		\
        tracker-marshal.h                             			\
        tracker-marshal.c

BUILT_SOURCES = 							\
	$(marshal_sources)

tracker_applet_SOURCES = 						\
	tracker-applet.c 						\
	tracker-applet.h						\
	tracker-marshal-main.c

icondir = $(datadir)/tracker/icons

icon_DATA = 								\
	tracker-applet-default.png 					\
	tracker-applet-indexing1.png 					\
	tracker-applet-indexing2.png 					\
	tracker-applet-paused.png

@INTLTOOL_DESKTOP_RULE@

autostartdir = $(sysconfdir)/xdg/autostart
autostart_DATA = tracker-applet.desktop

pkgdata_DATA = tracker-applet-prefs.glade


EXTRA_DIST = $(icon_DATA) $(pkgdata_DATA) tracker-marshal.list

CLEANFILES = $(autostart_DATA) $(BUILT_SOURCES)
