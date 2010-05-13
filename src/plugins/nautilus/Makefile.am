include $(top_srcdir)/Makefile.decl

INCLUDES =											\
	-I$(top_srcdir)/src									\
	-I$(top_builddir)/src									\
	-I$(top_builddir)/src/libtracker-client							\
	$(GCOV_CFLAGS)										\
	$(WARN_CFLAGS)										\
	$(NAUTILUS_EXTENSION_CFLAGS)								\
	$(TRACKER_APPS_CFLAGS)									\
	$(DBUS_CFLAGS)

nautilus_extensiondir = $(NAUTILUS_EXTENSION_INSTALL_DIR)

nautilus_extension_LTLIBRARIES = libnautilus-tracker-tags.la

libnautilus_tracker_tags_la_SOURCES =								\
	tracker-tags-extension.c								\
	tracker-tags-utils.c									\
	tracker-tags-utils.h									\
	tracker-tags-view.c									\
	tracker-tags-view.h

libnautilus_tracker_tags_la_LDFLAGS = -module -avoid-version
libnautilus_tracker_tags_la_LIBADD =								\
	$(top_builddir)/src/libtracker-client/libtracker-client-@TRACKER_API_VERSION@.la	\
	$(GCOV_LIBS)										\
	$(NAUTILUS_EXTENSION_LIBS)								\
	$(TRACKER_APPS_LIBS)
