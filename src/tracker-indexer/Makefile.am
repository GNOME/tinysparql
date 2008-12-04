include $(top_srcdir)/Makefile.decl

SUBDIRS = modules

INCLUDES =								\
	-DSHAREDIR=\""$(datadir)"\"					\
	-DLIBDIR=\""$(libdir)"\"					\
	-DLOCALEDIR=\""$(localedir)"\" 					\
	-DINDEXER_MODULES_DIR=\""$(libdir)/tracker/indexer-modules"\"	\
	-DLIBEXEC_PATH=\""$(libexecdir)"\"				\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	$(DBUS_CFLAGS)							\
	$(PANGO_CFLAGS)							\
	$(GMODULE_CFLAGS)

libtracker_indexerdir = $(libdir)/tracker
libtracker_indexerincludedir=$(includedir)/tracker-1.0/libtracker-indexer/
libtracker_indexer_LTLIBRARIES = libtracker-indexer.la

libtracker_indexer_la_SOURCES =	 					\
	tracker-module-file.c						\
	tracker-module-iteratable.c					\
	tracker-module-metadata-utils.c					\
	tracker-module-metadata.c

libtracker_indexerinclude_HEADERS =					\
	tracker-module-file.h						\
	tracker-module-iteratable.h					\
	tracker-module-metadata-utils.h					\
	tracker-module-metadata.h

libexec_PROGRAMS = tracker-indexer

tracker_indexer_SOURCES =						\
	tracker-dbus.c							\
	tracker-dbus.h							\
	tracker-indexer.c						\
	tracker-indexer.h						\
	tracker-indexer-module.c					\
	tracker-indexer-module.h					\
	tracker-main.c							\
	tracker-marshal-main.c						\
	tracker-module-metadata-private.h

tracker_indexer_LDADD =							\
	libtracker-indexer.la						\
	$(top_builddir)/src/libtracker-data/libtracker-data.la		\
	$(top_builddir)/src/libtracker-db/libtracker-db.la 		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(top_builddir)/src/libstemmer/libstemmer.la	 		\
	$(trackerd_win_libs)						\
	$(DBUS_LIBS)							\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(PANGO_LIBS)							\
	$(GIO_LIBS)							\
	$(GLIB2_LIBS)							\
	-lz								\
	-lm

#
# Note: This rule is slightly different from the normal rule we would
# use here because it copies the resulting file on success to the
# tracker-indexer directory in the unit tests so that they can build.
# We do this for the marshal generated source and for the dbus
# generated glue files.
#
# Should we add the files for the unit tests to CLEANFILES?
#

tracker-marshal.h: tracker-marshal.list
	$(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --header > $@ && \
	$(LN_S) -sf $(top_srcdir)/src/tracker-indexer/$@ $(top_srcdir)/tests/tracker-indexer/$@

tracker-marshal.c: tracker-marshal.list
	$(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --body > $@ && 	\
	$(LN_S) -sf $(top_srcdir)/src/tracker-indexer/$@ $(top_srcdir)/tests/tracker-indexer/$@

tracker-marshal-main.c: tracker-marshal.c tracker-marshal.h

marshal_sources =                                         		\
        tracker-marshal.h                             			\
        tracker-marshal.c

dbus_sources = 								\
	tracker-indexer-glue.h

%-glue.h: $(top_srcdir)/data/dbus/%.xml
	$(DBUSBINDINGTOOL) --mode=glib-server --output=$@ --prefix=$(subst -,_,$*) $^ && \
	$(LN_S) -sf $(top_srcdir)/src/tracker-indexer/$@ $(top_srcdir)/tests/tracker-indexer/$@

BUILT_SOURCES = 							\
	$(dbus_sources)							\
	$(marshal_sources)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = 								\
	$(BUILT_SOURCES)						\
	tracker-marshal.list
