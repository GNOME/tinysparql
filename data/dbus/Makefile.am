include $(top_srcdir)/Makefile.decl

configdir = $(datadir)/tracker

config_DATA =							\
	tracker-backup.xml					\
	tracker-extract.xml					\
	tracker-miner.xml					\
	tracker-miner-web.xml					\
	tracker-resources.xml					\
	tracker-resources-class.xml				\
	tracker-statistics.xml					\
	tracker-writeback.xml					\
	tracker-status.xml

config_noinst = 						\
	tracker-miner-files-reindex.xml

# Services
servicedir = $(DBUS_SERVICES_DIR)
service_in_files =						\
	org.freedesktop.Tracker1.service.in			\
	org.freedesktop.Tracker1.Miner.Applications.service.in	\
	org.freedesktop.Tracker1.Miner.Files.service.in		\
	org.freedesktop.Tracker1.Extract.service.in
service_DATA = $(service_in_files:.service.in=.service)

%.service: %.service.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|" $< > $@

EXTRA_DIST = 							\
	$(service_in_files)					\
	$(config_DATA)						\
	$(config_noinst)

CLEANFILES = $(service_DATA)
