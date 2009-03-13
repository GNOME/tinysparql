include $(top_srcdir)/Makefile.decl

if HAVE_TRACKER_SEARCH_TOOL
tst = tracker-search-tool.1
endif

if HAVE_TRACKER_APPLET
ta = tracker-applet.1
endif

if HAVE_TRACKER_PREFERENCES
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
	tracker-services.1	\
	tracker-info.1		\
	tracker-unique.1	\
	$(tst)			\
	$(ta)			\
	$(tp)

EXTRA_DIST = $(man_MANS)	
