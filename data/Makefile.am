SUBDIRS = english

configdir = $(datadir)/tracker

config_DATA = mysql-tracker.sql tracker.cfg tracker-stop-words.txt

servicedir       = $(prefix)/share/dbus-1/services
service_in_files = tracker.service.in
service_DATA     = tracker.service

%.service: %.service.in
	@sed -e "s|\@bindir\@|$(bindir)|" $< > $@


EXTRA_DIST = $(config_DATA) tracker-introspect.xml
