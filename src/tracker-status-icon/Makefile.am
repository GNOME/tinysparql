include $(top_srcdir)/Makefile.decl

INCLUDES =								\
	-DICONS_DIR=\""$(datadir)/tracker/icons/"\"			\
	-DSHAREDIR=\""$(datadir)"\"					\
	-DLOCALEDIR=\""$(localedir)"\" 					\
	-DTRACKER_LIBEXECDIR=\""$(libexecdir)"\" 			\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	$(TRACKER_APPS_CFLAGS)						\
	$(WARN_CFLAGS)							\
	$(GCOV_CFLAGS)

bin_PROGRAMS = tracker-status-icon

tracker_status_icon_LDADD = 						\
	$(top_builddir)/src/libtracker-miner/libtracker-miner.la 	\
	$(TRACKER_APPS_LIBS)						\
	$(GCOV_LIBS)

tracker_status_icon_SOURCES =						\
	tracker-status-icon.c						\
	tracker-status-icon.h						\
	tracker-main.c

icondir = $(datadir)/tracker/icons

icon_DATA = 								\
	tracker-applet-default.png 					\
	tracker-applet-indexing1.png 					\
	tracker-applet-indexing2.png 					\
	tracker-applet-paused.png

@INTLTOOL_DESKTOP_RULE@

autostartdir = $(sysconfdir)/xdg/autostart
autostart_DATA = tracker-status-icon.desktop

EXTRA_DIST = $(icon_DATA)

CLEANFILES = $(autostart_DATA)
