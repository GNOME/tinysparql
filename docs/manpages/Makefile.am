include $(top_srcdir)/Makefile.decl

if HAVE_TRACKER_SEARCH_BAR
tsb = tracker-search-bar.1
endif

if HAVE_TRACKER_SEARCH_TOOL
tst = tracker-search-tool.1
endif

if HAVE_TRACKER_STATUS_ICON
ta = tracker-status-icon.1
endif

if HAVE_TRACKER_PREFERENCES
tp = tracker-preferences.1
endif

man_MANS = 				\
	tracker-extract.1		\
	tracker-extract.cfg.5		\
	tracker-fts.cfg.5		\
	tracker-import.1		\
	tracker-info.1			\
	tracker-miner-fs.1		\
	tracker-miner-fs.cfg.5		\
	tracker-control.1		\
	tracker-search.1		\
	tracker-sparql.1		\
	tracker-stats.1			\
	tracker-status.1		\
	tracker-store.1			\
	tracker-store.cfg.5		\
	tracker-tag.1			\
	$(tsb)				\
	$(tst)				\
	$(ta)				\
	$(tp)

EXTRA_DIST = $(man_MANS)
