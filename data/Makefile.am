SUBDIRS = languages

configdir = $(datadir)/tracker

config_DATA =   tracker-introspect.xml \
		sqlite-tracker.sql \
		sqlite-stored-procs.sql

servicedir       = $(DBUS_SERVICES_DIR)
service_in_files = tracker.service.in
service_DATA     = tracker.service

CLEANFILES = $(service_DATA)
EXTRA_DIST = $(config_DATA) $(service_in_files)

%.service: %.service.in
	@sed -e "s|\@bindir\@|$(bindir)|" $< > $@
