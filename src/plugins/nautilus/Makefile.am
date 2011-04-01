include $(top_srcdir)/Makefile.decl

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	$(TRACKER_NAUTILUS_EXTENSION_CFLAGS)

nautilus_extensiondir = $(TRACKER_NAUTILUS_EXTENSION_INSTALL_DIR)

nautilus_extension_LTLIBRARIES = libnautilus-tracker-tags.la

libnautilus_tracker_tags_la_SOURCES =                  \
	tracker-tags-extension.c                       \
	tracker-tags-utils.c                           \
	tracker-tags-utils.h                           \
	tracker-tags-view.c                            \
	tracker-tags-view.h

libnautilus_tracker_tags_la_LDFLAGS = -module -avoid-version
libnautilus_tracker_tags_la_LIBADD =                   \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(BUILD_LIBS)                                  \
	$(TRACKER_NAUTILUS_EXTENSION_LIBS)
