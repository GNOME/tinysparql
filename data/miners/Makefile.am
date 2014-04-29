desktop_in_in_files =                                  \
	tracker-miner-extract.desktop.in.in            \
	tracker-miner-userguides.desktop.in.in         \
	tracker-miner-applications.desktop.in.in       \
	tracker-miner-evolution.desktop.in.in          \
	tracker-miner-files.desktop.in.in              \
	tracker-miner-rss.desktop.in.in

desktop_in_files =                                     \
	tracker-miner-extract.desktop.in               \
	tracker-miner-userguides.desktop.in            \
	tracker-miner-applications.desktop.in          \
	tracker-miner-evolution.desktop.in             \
	tracker-miner-files.desktop.in                 \
	tracker-miner-rss.desktop.in

tracker_minersdir = $(datadir)/tracker/miners

tracker_miners_DATA =

if HAVE_TRACKER_MINER_FS
tracker_miners_DATA +=                                 \
	tracker-miner-extract.desktop                  \
	tracker-miner-files.desktop
endif

if HAVE_TRACKER_MINER_APPS
tracker_miners_DATA += tracker-miner-applications.desktop
endif

if HAVE_TRACKER_MINER_USER_GUIDES
tracker_miners_DATA += tracker-miner-userguides.desktop
endif

if HAVE_TRACKER_MINER_EVOLUTION
tracker_miners_DATA += tracker-miner-evolution.desktop
endif

if HAVE_TRACKER_MINER_RSS
tracker_miners_DATA += tracker-miner-rss.desktop
endif

@INTLTOOL_DESKTOP_RULE@

%.desktop.in: %.desktop.in.in
	@sed -e "s|@datadir[@]|$(datadir)|" $< > $@

EXTRA_DIST = $(desktop_in_in_files)
CLEANFILES =                                           \
	$(tracker_miners_DATA)                         \
	$(desktop_in_files)
