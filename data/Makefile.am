SUBDIRS = languages icons services

configdir = $(datadir)/tracker

config_DATA =   tracker-introspect.xml \
		sqlite-tracker.sql \
		sqlite-cache.sql \
		sqlite-email.sql \
		sqlite-service-stored-procs.sql \
		sqlite-service-triggers.sql \
		sqlite-tracker-triggers.sql \
		sqlite-user-data.sql \
		sqlite-stored-procs.sql \
		sqlite-service.sql	\
		sqlite-service-types.sql \
		sqlite-metadata.sql 

servicedir       = $(DBUS_SERVICES_DIR)
service_in_files = tracker.service.in
service_DATA     = tracker.service

CLEANFILES = $(service_DATA)
EXTRA_DIST = $(config_DATA) $(service_in_files)

%.service: %.service.in
	@sed -e "s|\@bindir\@|$(bindir)|" $< > $@
