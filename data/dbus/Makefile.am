include $(top_srcdir)/Makefile.decl

configdir = $(datadir)/tracker

config_DATA =						\
	tracker-backup.xml				\
	tracker-daemon.xml				\
	tracker-resources.xml				\
	tracker-search.xml				\
	tracker-indexer.xml				\
	tracker-extract.xml				\
	tracker-resources-class.xml

# Services
servicedir = $(DBUS_SERVICES_DIR)
service_in_files =					\
	org.freedesktop.Tracker.service.in		\
	org.freedesktop.Tracker.Indexer.service.in	\
	org.freedesktop.Tracker.Extract.service.in
service_DATA = $(service_in_files:.service.in=.service)

%.service: %.service.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|" $< > $@

EXTRA_DIST = 						\
	$(service_in_files)				\
	$(config_DATA)

CLEANFILES = $(service_DATA)	
