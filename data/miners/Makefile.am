include $(top_srcdir)/Makefile.decl

desktop_in_files = \
	tracker-miner-applications.desktop.in \
	tracker-miner-files.desktop.in

tracker_minersdir = $(datadir)/tracker/miners

tracker_miners_DATA = \
	tracker-miner-applications.desktop \
	tracker-miner-files.desktop

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = $(desktop_in_files)
CLEANFILES = $(tracker_miners_DATA)
