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
service_in_files =						\
	org.freedesktop.Tracker1.service.in			\
	org.freedesktop.Tracker1.Miner.Applications.service.in	\
	org.freedesktop.Tracker1.Miner.EMails.service.in	\
	org.freedesktop.Tracker1.Miner.Files.service.in		\
	org.freedesktop.Tracker1.Miner.Flickr.service.in	\
	org.freedesktop.Tracker1.Miner.RSS.service.in		\
	org.freedesktop.Tracker1.Extract.service.in

%.service: %.service.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|" $< > $@

# Services
servicedir = $(DBUS_SERVICES_DIR)
service_DATA =							\
	org.freedesktop.Tracker1.service			\
	org.freedesktop.Tracker1.Miner.Applications.service	\
	org.freedesktop.Tracker1.Miner.Files.service		\
	org.freedesktop.Tracker1.Extract.service

if HAVE_MINER_EVOLUTION
service_DATA += org.freedesktop.Tracker1.Miner.EMails.service
endif

if HAVE_MINER_RSS
service_DATA += org.freedesktop.Tracker1.Miner.RSS.service
endif

if HAVE_MINER_FLICKR
service_DATA += org.freedesktop.Tracker1.Miner.Flickr.service
endif


EXTRA_DIST = 							\
	$(service_in_files)					\
	$(config_DATA)						\
	$(config_noinst)

CLEANFILES = $(service_DATA)
