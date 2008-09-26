include $(top_srcdir)/Makefile.decl

if HAVE_GNOME
tst = tracker-search-tool.1
endif

if ENABLE_TRACKERAPPLET
ta = tracker-applet.1
endif

if ENABLE_PREFERENCES
tp = tracker-preferences.1
endif

man_MANS = 			\
	tracker-extract.1	\
	tracker-files.1		\
	tracker-meta-folder.1	\
	tracker-query.1		\
	tracker-search.1	\
	tracker-stats.1		\
	trackerd.1		\
	tracker-thumbnailer.1	\
	tracker-tag.1		\
	tracker-status.1	\
	tracker.cfg.5		\
	tracker-services.7	\
	$(tst)			\
	$(ta)			\
	$(tp)

EXTRA_DIST = $(man_MANS)
