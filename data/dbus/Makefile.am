include $(top_srcdir)/Makefile.decl

configdir = $(datadir)/tracker

config_DATA =						\
	tracker-backup.xml				\
	tracker-extract.xml				\
	tracker-miner.xml				\
	tracker-resources.xml				\
	tracker-resources-class.xml			\
	tracker-search.xml				\
	tracker-statistics.xml

# Services
servicedir = $(DBUS_SERVICES_DIR)
service_in_files =					\
	org.freedesktop.Tracker.service.in		\
	org.freedesktop.Tracker.Miner.FS.service.in	\
	org.freedesktop.Tracker.Extract.service.in
service_DATA = $(service_in_files:.service.in=.service)

%.service: %.service.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|" $< > $@

EXTRA_DIST = 						\
	$(service_in_files)				\
	$(config_DATA)

CLEANFILES = $(service_DATA)	
