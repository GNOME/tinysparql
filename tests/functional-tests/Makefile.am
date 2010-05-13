include $(top_srcdir)/Makefile.decl

SUBDIRS = data

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

EXTRA_DIST = 					\
	$(config_SCRIPTS) 			\
	$(bashscripts_SCRIPTS)			\
	$(bashscripts_in_files)

CLEANFILES = $(bashscripts_SCRIPTS)
