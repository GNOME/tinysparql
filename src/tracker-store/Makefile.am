include $(top_srcdir)/Makefile.decl

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-DSHAREDIR=\""$(datadir)"\"                    \
	-DLOCALEDIR=\""$(localedir)"\"                 \
	-DMAIL_MODULES_DIR=\""$(libdir)"/tracker-$(TRACKER_API_VERSION)/mail-modules\" \
	-DPUSH_MODULES_DIR=\""$(libdir)/tracker-$(TRACKER_API_VERSION)/push-modules/daemon"\" \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	$(TRACKER_STORE_CFLAGS)                        \
	-include config.h                              \
	-DNO_LIBDBUS

#
# Daemon sources
#
libexec_PROGRAMS = tracker-store

tracker_store_SOURCES =                                \
	tracker-backup.vala                            \
	tracker-config.c                               \
	tracker-dbus.vala                              \
	tracker-events.c                               \
	tracker-locale-change.c                        \
	tracker-main.vala                              \
	tracker-resources.vala                         \
	tracker-statistics.vala                        \
	tracker-status.vala                            \
	tracker-steroids.vala                          \
	tracker-store.vala                             \
	tracker-writeback.c

noinst_HEADERS =                                       \
	tracker-config.h                               \
	tracker-events.h                               \
	tracker-locale-change.h                        \
	tracker-store.h                                \
	tracker-writeback.h

BUILT_SOURCES = tracker-store.h

tracker_store_VALAFLAGS = \
	--pkg gio-2.0 \
	--pkg gio-unix-2.0 \
	--pkg posix \
	$(BUILD_VALAFLAGS) \
	$(top_srcdir)/src/libtracker-common/libtracker-common.vapi \
	$(top_srcdir)/src/libtracker-sparql/tracker-sparql-$(TRACKER_API_VERSION).vapi \
	$(top_srcdir)/src/libtracker-data/tracker-sparql-query.vapi \
	$(top_srcdir)/src/libtracker-data/libtracker-data.vapi \
	$(top_srcdir)/src/tracker-store/tracker-config.vapi \
	$(top_srcdir)/src/tracker-store/tracker-events.vapi \
	$(top_srcdir)/src/tracker-store/tracker-locale-change.vapi \
	$(top_srcdir)/src/tracker-store/tracker-writeback.vapi \
	-H tracker-store.h

tracker_store_LDADD =                                  \
	$(top_builddir)/src/libtracker-data/libtracker-data.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(BUILD_LIBS)                                  \
	$(TRACKER_STORE_LIBS)

EXTRA_DIST = \
	tracker-config.vapi \
	tracker-events.vapi \
	tracker-locale-change.vapi \
	tracker-writeback.vapi
