common = \
	tracker-info.1 \
	tracker-daemon.1 \
	tracker-search.1 \
	tracker-sparql.1 \
	tracker-sql.1 \
	tracker-status.1 \
	tracker-store.1 \
	tracker-tag.1 \
	tracker-reset.1 \
	tracker-index.1

# Require only common and enabled manpages
man_MANS = $(common)

# Make sure even disabled manpages get distributed
EXTRA_DIST = \
	$(common) \
	meson.build
