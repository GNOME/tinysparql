tp = tracker-preferences.1
tn = tracker-needle.1
tmrss = tracker-miner-rss.1

common = \
	tracker-extract.1 \
	tracker-import.1 \
	tracker-info.1 \
	tracker-miner-fs.1 \
	tracker-control.1 \
	tracker-search.1 \
	tracker-sparql.1 \
	tracker-stats.1 \
	tracker-store.1 \
	tracker-tag.1 \
	tracker-writeback.1

# Require only common and enabled manpages
man_MANS = $(common)

if HAVE_TRACKER_PREFERENCES
man_MANS += $(tp)
endif

if HAVE_TRACKER_NEEDLE
man_MANS += $(tn)
endif

if HAVE_TRACKER_MINER_RSS
man_MANS += $(tmrss)
endif

# Make sure even disabled manpages get distributed
EXTRA_DIST = \
	$(common) \
	$(tp) \
	$(tn) \
	$(tmrss)
