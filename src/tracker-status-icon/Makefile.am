include $(top_srcdir)/Makefile.decl

INCLUDES =								\
	-DICONS_DIR=\""$(datadir)/tracker/icons/"\"			\
	-DSHAREDIR=\""$(datadir)"\"					\
	-DLOCALEDIR=\""$(localedir)"\" 					\
	-DTRACKER_LIBEXECDIR=\""$(libexecdir)"\" 			\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	-I$(top_builddir)/src						\
	-I$(top_builddir)/src/libtracker-client				\
	$(TRACKER_APPS_CFLAGS)						\
	$(PANGO_CFLAGS)							\
	$(WARN_CFLAGS)							\
	$(GCOV_CFLAGS)

bin_PROGRAMS = tracker-status-icon

# Note: the -lX11 is needed for XGetModifierMapping() and other X11 calls
tracker_status_icon_LDADD = 						\
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la 	\
	$(top_builddir)/src/libtracker-client/libtracker-client-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(TRACKER_APPS_LIBS)						\
	$(PANGO_LIBS)							\
	$(GCOV_LIBS)							\
	-lX11

tracker_status_icon_SOURCES =						\
	tomboykeybinder.c						\
	tomboykeybinder.h						\
	tracker-icon-config.c						\
	tracker-icon-config.h						\
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
