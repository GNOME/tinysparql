include $(top_srcdir)/Makefile.decl

if HAVE_TRACKER_SEARCH_BAR
tsb = tracker-search-bar.1
endif

if HAVE_TRACKER_PREFERENCES
tp = tracker-preferences.1
endif

if HAVE_TRACKER_NEEDLE
tn = tracker-needle.1
endif

if HAVE_TRACKER_MINER_RSS
tmrss = tracker-miner-rss.1
endif

common = \
	tracker-extract.1 \
	tracker-extract.cfg.5 \
	tracker-fts.cfg.5 \
	tracker-import.1 \
	tracker-info.1 \
	tracker-miner-fs.1 \
	tracker-miner-fs.cfg.5 \
	tracker-control.1 \
	tracker-search.1 \
	tracker-sparql.1 \
	tracker-stats.1 \
	tracker-store.1 \
	tracker-store.cfg.5 \
	tracker-tag.1 \
	tracker-writeback.1

# Install only common and enabled manpages
man_MANS = \
	$(common) \
	$(tmrss) \
	$(tsb) \
	$(tn) \
	$(tp)

# Make sure even disabled manpages get distributed
EXTRA_DIST = \
	$(common) \
	tracker-miner-rss.1 \
	tracker-search-bar.1 \
	tracker-preferences.1 \
	tracker-needle.1
