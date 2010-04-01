include $(top_srcdir)/Makefile.decl

icondir = $(datadir)/icons/hicolor/scalable/apps
icon_DATA = tracker.svg

minericonsdir = $(datadir)/tracker/icons
minericons_DATA =


if HAVE_MINER_TWITTER
minericons_DATA += \
	tracker-miner-twitter.svg \
	tracker-miner-identica.svg
endif

EXTRA_DIST = $(icon_DATA)
