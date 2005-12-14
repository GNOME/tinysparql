SUBDIRS = english

configdir = $(datadir)/tracker

config_DATA = mysql-tracker.sql tracker.cfg tracker-stop-words.txt

#servicedir = "/usr/share/dbus-1/services"

#service_in_files = tracker.service.in

#service_DATA = $(service_in_files:.service.in=.service)

# Rule to make the service file with bindir expanded
#$(service_DATA): $(service_in_files) Makefile
#	@sed -e "s|\@bindir\@|$(bindir)|" $< > $@


EXTRA_DIST = $(config_DATA) tracker-introspect.xml
