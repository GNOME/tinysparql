SUBDIRS = english

configdir = $(datadir)/tracker

config_DATA =   tracker-introspect.xml \
		mysql-tracker.sql \
		mysql-stored-procs.sql \
		mysql-system.sql \
		tracker.cfg \
		tracker-stop-words.txt

servicedir       = $(prefix)/share/dbus-1/services
service_in_files = tracker.service.in
service_DATA     = tracker.service

EXTRA_DIST = $(config_DATA) $(service_in_files)

%.service: %.service.in
	@sed -e "s|\@bindir\@|$(bindir)|" $< > $@
