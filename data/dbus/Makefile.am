include $(top_srcdir)/Makefile.decl

configdir = $(datadir)/tracker

config_DATA = \
	tracker-backup.xml \
	tracker-extract.xml \
	tracker-miner.xml \
	tracker-miner-web.xml \
	tracker-resources.xml \
	tracker-statistics.xml \
	tracker-writeback.xml \
	tracker-status.xml

config_noinst = \
	tracker-miner-files-index.xml

# Services
service_in_files = \
	org.freedesktop.Tracker1.service.in \
	org.freedesktop.Tracker1.Miner.Applications.service.in \
	org.freedesktop.Tracker1.Miner.EMails.service.in \
	org.freedesktop.Tracker1.Miner.Files.service.in \
	org.freedesktop.Tracker1.Miner.Userguides.service.in \
	org.freedesktop.Tracker1.Miner.RSS.service.in  \
	org.freedesktop.Tracker1.Miner.Extract.service.in \
	org.freedesktop.Tracker1.Writeback.service.in

%.service: %.service.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|" $< > $@

# Services
servicedir = $(DBUS_SERVICES_DIR)
service_DATA = \
	org.freedesktop.Tracker1.service \
	org.freedesktop.Tracker1.Writeback.service

if HAVE_TRACKER_MINER_FS
service_DATA += \
	org.freedesktop.Tracker1.Miner.Extract.service \
	org.freedesktop.Tracker1.Miner.Files.service \
	org.freedesktop.Tracker1.Miner.Applications.service

if HAVE_MAEMO
# Userguides are done in the miner-fs process
service_DATA += org.freedesktop.Tracker1.Miner.Userguides.service
endif
endif

if HAVE_TRACKER_MINER_EVOLUTION
service_DATA += org.freedesktop.Tracker1.Miner.EMails.service
endif

if HAVE_TRACKER_MINER_RSS
service_DATA += org.freedesktop.Tracker1.Miner.RSS.service
endif

EXTRA_DIST = \
	$(service_in_files) \
	$(config_DATA) \
	$(config_noinst)

CLEANFILES = $(service_DATA)
