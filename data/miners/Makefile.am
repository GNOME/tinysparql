include $(top_srcdir)/Makefile.decl

desktop_in_in_files =                                  \
	tracker-miner-userguides.desktop.in.in         \
	tracker-miner-applications.desktop.in.in       \
	tracker-miner-evolution.desktop.in.in          \
	tracker-miner-files.desktop.in.in              \
	tracker-miner-flickr.desktop.in.in             \
	tracker-miner-rss.desktop.in.in

desktop_in_files =                                     \
	tracker-miner-userguides.desktop.in            \
	tracker-miner-applications.desktop.in          \
	tracker-miner-evolution.desktop.in             \
	tracker-miner-files.desktop.in                 \
	tracker-miner-flickr.desktop.in                \
	tracker-miner-rss.desktop.in

tracker_minersdir = $(datadir)/tracker/miners

tracker_miners_DATA =                                  \
	tracker-miner-applications.desktop             \
	tracker-miner-files.desktop

if HAVE_MAEMO
tracker_miners_DATA += tracker-miner-userguides.desktop
endif

if HAVE_TRACKER_MINER_EVOLUTION
tracker_miners_DATA += tracker-miner-evolution.desktop
endif

if HAVE_TRACKER_MINER_RSS
tracker_miners_DATA += tracker-miner-rss.desktop
endif

if HAVE_TRACKER_MINER_FLICKR
tracker_miners_DATA += tracker-miner-flickr.desktop
endif

@INTLTOOL_DESKTOP_RULE@

%.desktop.in: %.desktop.in.in
	@sed -e "s|@datadir[@]|$(datadir)|" $< > $@

EXTRA_DIST = $(desktop_in_in_files)
CLEANFILES =                                           \
	$(tracker_miners_DATA)                         \
	$(desktop_in_files)
