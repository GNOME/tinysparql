bin_PROGRAMS = tracker-explorer

tracker_explorer_VALASOURCES = \
	explorer.vala \
	$(NULL)


tracker_explorer_SOURCES = \
	$(tracker_explorer_VALASOURCES:.vala=.c) \
	$(NULL)

tracker_explorer.vala.stamp: $(tracker_explorer_VALASOURCES)
	$(VALAC) -C -g --pkg dbus-glib-1 --pkg gee-1.0 --pkg gtk+-2.0 $^
	touch tracker_explorer.vala.stamp

tracker_explorer_CFLAGS = \
	$(WARN_CFLAGS)						\
	$(GLIB2_CFLAGS)						\
	$(GCOV_CFLAGS)						\
	$(GIO_CFLAGS)						\
	$(DBUS_CFLAGS)						\
	$(LIBTRACKERGTK_CFLAGS)					\
	$(LIBGEE_CFLAGS)					\
	$(NULL)

tracker_explorer_LDADD = \
	$(GLIB2_LIBS)						\
	$(GIO_LIBS)						\
	$(GCOV_LIBS)						\
	$(DBUS_LIBS)						\
	$(LIBTRACKERGTK_LIBS)					\
	$(LIBGEE_LIBS)						\
	$(NULL)

EXTRA_DIST = \
	$(tracker_explorer_SOURCES) \
	$(tracker_explorer_VALASOURCES) \
	tracker_explorer.vala.stamp \
	$(NULL)

DISTCLEANFILES = \
	*.gcno \
	*.gcda \
	$(NULL)

MAINTAINERCLEANFILES = \
	$(tracker_explorer_SOURCES) \
	tracker_explorer.vala.stamp \
	$(NULL)

BUILT_SOURCES = \
	tracker_explorer.vala.stamp \
	$(NULL)

