include $(top_srcdir)/Makefile.decl

configdir = $(datadir)/tracker

config_DATA =						\
	tracker-daemon.xml				\
	tracker-files.xml				\
	tracker-keywords.xml				\
	tracker-metadata.xml				\
	tracker-search.xml				\
	tracker-xesam.xml				\
	tracker-indexer.xml

# Services
org.freedesktop.Tracker_servicedir = $(DBUS_SERVICES_DIR)
org.freedesktop.Tracker_service_DATA = org.freedesktop.Tracker.service

org.freedesktop.Tracker.Indexer_servicedir = $(DBUS_SERVICES_DIR)
org.freedesktop.Tracker.Indexer_service_DATA = org.freedesktop.Tracker.Indexer.service

%.service: %.service.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|" $< > $@

EXTRA_DIST = 						\
	$(org.freedesktop.Tracker_service_DATA)		\
	$(org.freedesktop.Tracker.Indexer_service_DATA)	\
	$(config_DATA)

CLEANFILES = 						\
	$(org.freedesktop.Tracker_service_DATA)		\
	$(org.freedesktop.Tracker.Indexer_service_DATA)
