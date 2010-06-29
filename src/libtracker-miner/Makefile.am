include $(top_srcdir)/Makefile.decl

# Include list of shared sources:
#  Defines $(libtracker_miner_marshal_sources) and
#  $(libtracker_miner_monitor_sources)
include Makefile-shared-sources.decl

INCLUDES =						\
	-DSHAREDIR=\""$(datadir)"\"			\
	-DDBUS_SERVICES_DIR=\""$(DBUS_SERVICES_DIR)"\"	\
	-DG_LOG_DOMAIN=\"Tracker\"			\
	-DTRACKER_COMPILATION				\
	-DTRACKER_MINERS_DIR=\""$(datadir)/tracker/miners"\" 	\
	-I$(top_srcdir)/src				\
	-I$(top_builddir)/src				\
	-I$(top_builddir)/src/libtracker-client		\
	$(WARN_CFLAGS)					\
	$(GLIB2_CFLAGS)					\
	$(GCOV_CFLAGS)					\
	$(GIO_CFLAGS)					\
	$(DBUS_CFLAGS)

lib_LTLIBRARIES = libtracker-miner-@TRACKER_API_VERSION@.la

libtracker_minerdir = $(libdir)/tracker-$(TRACKER_API_VERSION)
libtracker_minerincludedir=$(includedir)/tracker-$(TRACKER_API_VERSION)/libtracker-miner/

libtracker_miner_@TRACKER_API_VERSION@_la_SOURCES = 	\
	$(libtracker_miner_marshal_sources)		\
	$(libtracker_miner_monitor_sources)		\
	tracker-crawler.c				\
	tracker-dbus.c					\
	tracker-dbus.h					\
	tracker-miner-dbus.h 				\
	tracker-miner-object.c				\
	tracker-miner-fs.c				\
	tracker-miner-manager.c				\
	tracker-miner-web.c				\
	tracker-miner-web-dbus.h			\
	tracker-network-provider.c			\
	tracker-password-provider.c			\
	tracker-storage.c				\
	tracker-thumbnailer.c				\
	tracker-utils.c					\
	tracker-utils.h

libtracker_minerinclude_HEADERS = 			\
	tracker-crawler.h				\
	tracker-miner.h					\
	tracker-miner-object.h 				\
	tracker-miner-fs.h				\
	tracker-miner-manager.h				\
	tracker-miner-web.h				\
	tracker-network-provider.h			\
	tracker-password-provider.h			\
	tracker-storage.h				\
	tracker-thumbnailer.h

libtracker_miner_@TRACKER_API_VERSION@_la_LDFLAGS = 	\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-symbols-regex '^tracker_(miner|thumbnailer|crawler|storage|password_provider|network_provider)_.*'

libtracker_miner_@TRACKER_API_VERSION@_la_LIBADD = 	\
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(top_builddir)/src/libtracker-client/libtracker-client-@TRACKER_API_VERSION@.la \
	$(DBUS_LIBS)					\
	$(GIO_LIBS)					\
	$(GCOV_LIBS)					\
	$(GLIB2_LIBS)

if HAVE_GNOME_KEYRING
libtracker_miner_@TRACKER_API_VERSION@_la_SOURCES += 	\
	tracker-password-provider-gnome.c

libtracker_miner_@TRACKER_API_VERSION@_la_LIBADD +=	\
	$(GNOME_KEYRING_LIBS)

INCLUDES +=						\
	$(GNOME_KEYRING_CFLAGS)
else
libtracker_miner_@TRACKER_API_VERSION@_la_SOURCES += 	\
	tracker-password-provider-keyfile.c
endif

if HAVE_NETWORK_MANAGER
libtracker_miner_@TRACKER_API_VERSION@_la_SOURCES +=	\
	tracker-network-provider-network-manager.c

libtracker_miner_@TRACKER_API_VERSION@_la_LIBADD +=	\
	$(NETWORK_MANAGER_LIBS)

INCLUDES +=						\
	$(NETWORK_MANAGER_CFLAGS)
else
libtracker_miner_@TRACKER_API_VERSION@_la_SOURCES +=	\
	tracker-network-provider-dummy.c
endif

$(top_builddir)/src/libtracker-miner/tracker-marshal.h: tracker-marshal.list
	$(AM_V_GEN)$(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --header > $@

$(top_builddir)/src/libtracker-miner/tracker-marshal.c: tracker-marshal.list
	$(AM_V_GEN)(echo "#include \"tracker-marshal.h\""; \
		    $(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --body) > $@

# Generate DBus files from XML data.
dbus_sources = 						\
	tracker-miner-glue.h				\
	tracker-miner-web-glue.h			\
	tracker-miner-client.h

%-glue.h: $(top_srcdir)/data/dbus/%.xml
	$(AM_V_GEN)$(DBUSBINDINGTOOL) --mode=glib-server --output=$@ --prefix=_$(subst -,_,$*) $^

%-client.h: $(top_srcdir)/data/dbus/%.xml
	$(AM_V_GEN)$(DBUSBINDINGTOOL) --mode=glib-client --output=$@ --prefix=_$(subst -,_,$*) $^

# Vala bindings
vapidir = $(datadir)/vala/vapi
vapi_DATA =                     			\
	tracker-miner-$(TRACKER_API_VERSION).vapi	\
	tracker-miner-$(TRACKER_API_VERSION).deps

tracker-miner-$(TRACKER_API_VERSION).vapi: tracker-miner.vapi
	cp $< $@

tracker-miner-$(TRACKER_API_VERSION).deps: tracker-miner.deps
	cp $< $@

# Custom rule to avoid API duplication. There is also a workaround for bug
# in dbus-binding-tool where it generates bad code when two files are passed
# on the command line (though the man page says it supports it)
# This bug is reported at https://bugs.freedesktop.org/show_bug.cgi?id=25056
tracker-miner-web-glue.h: $(top_srcdir)/data/dbus/tracker-miner-web.xml $(top_srcdir)/data/dbus/tracker-miner.xml
	echo '<?xml version="1.0" encoding="UTF-8"?>'        > tracker-miner-web-full.xml
	echo '<node name="/">'                              >> tracker-miner-web-full.xml
	cat $^ | grep -v -e '<node' -e '<?xml' -e '</node>' >> tracker-miner-web-full.xml
	echo '</node>'                                      >> tracker-miner-web-full.xml
	$(AM_V_GEN)$(DBUSBINDINGTOOL) --mode=glib-server --output=$@ --prefix=_tracker_miner_web_dbus tracker-miner-web-full.xml

tracker-miner-web-full.xml: tracker-miner-web-glue.h

BUILT_SOURCES = $(dbus_sources) $(libtracker_miner_marshal_sources) $(vapi_DATA)

CLEANFILES = $(BUILT_SOURCES)

DISTCLEANFILES = tracker-miner-web-full.xml

EXTRA_DIST = 				\
	tracker-marshal.list		\
	tracker-miner.vapi		\
	tracker-miner.deps		\
	tracker-miner-web-full.xml
