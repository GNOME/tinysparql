include $(top_srcdir)/Makefile.decl

SUBDIRS = data

INCLUDES =					\
	$(WARN_CFLAGS)				\
	$(DBUS_CFLAGS)				\
	$(GLIB2_CFLAGS)				\
	$(GIO_CFLAGS)				\
	$(GTHREAD_CFLAGS)

configdir = $(datadir)/tracker-tests

config_SCRIPTS =			\
	01-insertion.py			\
	02-metacontacts.py		\
	03-fts-functions.py		\
	04-group-concat.py		\
	05-coalesce.py			\
	06-distance.py			\
	07-graph.py			\
	08-unique-insertions.py		\
	09-concurrent-query.py		\
	backup-restore-tc.py		\
	bootup-tc.py			\
	configuration.py		\
	metadata_extraction_tc.py	\
	mass-storage-mode.py		\
	fts-tc.py			\
	miner-basic-ops.py		\
	performance-tc.py		\
	tests.xml			\
	virtual-files-tc.py		\
	writeback-tc.py

bashscriptsdir = $(datadir)/tracker-tests

bashscripts_in_files =				\
	force-sqlite-misused.sh.in		\
	force-sqlite-misused-batch.sh.in	\
	helper-test-data.sh.in

bashscripts_SCRIPTS = 				\
	$(bashscripts_in_files:.sh.in=.sh)

%.sh: %.sh.in
	@sed -e "s|@topsrcdir[@]|${top_srcdir}|" \
	     -e "s|@libexecdir[@]|${libexecdir}|" $< > $@

noinst_PROGRAMS = busy-handling-test

busy_handling_test_VALASOURCES = busy-handling-test.vala

busy_handling_test_SOURCES = \
	busy-handling-test.vala.stamp \
	$(busy_handling_test_VALASOURCES:.vala=.c)

busy-handling-test.vala.stamp: $(busy_handling_test_VALASOURCES)
	$(AM_V_GEN)$(VALAC) $(GCOV_VALAFLAGS) -C $(VALAFLAGS) --pkg gio-2.0 --pkg dbus-glib-1 $^
	$(AM_V_GEN)touch $@

busy_handling_test_LDADD = \
	$(GIO_LIBS) \
	$(GLIB2_LIBS) \
	$(DBUS_LIBS)

BUILT_SOURCES = 				\
	busy-handling-test.vala.stamp

MAINTAINERCLEANFILES =					\
	$(BUILT_SOURCES)				\
	$(busy_handling_test_VALASOURCES:.vala=.c)

EXTRA_DIST = 					\
	$(config_SCRIPTS) 			\
	$(bashscripts_SCRIPTS)			\
	$(bashscripts_in_files)			\
	$(BUILT_SOURCES)			\
	$(busy_handling_test_VALASOURCES)

CLEANFILES = $(bashscripts_SCRIPTS)

