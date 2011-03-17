include $(top_srcdir)/Makefile.decl

SUBDIRS = \
	ipc \
	common \
	test-configurations \
	test-extraction-data \
	test-writeback-data \
	test-apps-data \
	ttl \
	unittest2 \
	test-ontologies

configdir = $(datadir)/tracker-tests

config_SCRIPTS = \
	__init__.py \
	$(slow_tests) \
	$(standard_tests)

if HAVE_MAEMO
config_SCRIPTS += \
	tests.xml \
	tracker-tests.aegis
endif

dconfdir = $(sysconfdir)/dconf/profile
dconf_DATA = \
	tracker-test

trackertest: trackertest.dconf.in
	cp trackertest.dconf.in trackertest

standard_tests = \
	01-insertion.py \
	02-sparql-bugs.py
if HAVE_TRACKER_FTS
standard_tests += 03-fts-functions.py
endif
standard_tests += \
	04-group-concat.py \
	05-coalesce.py \
	06-distance.py \
	07-graph.py \
	08-unique-insertions.py \
	09-concurrent-query.py \
	14-signals.py \
	15-statistics.py \
	16-collation.py \
	17-ontology-changes.py  \
	200-backup-restore.py \
	300-miner-basic-ops.py
if HAVE_TRACKER_FTS
standard_tests += 310-fts-indexing.py
endif
standard_tests += \
	400-extractor.py \
	500-writeback.py \
	501-writeback-details.py \
	600-applications-camera.py \
	601-applications-sync.py \
	mass-storage-mode.py \
	performance-tc-modified.py \
	virtual-files-tc.py

slow_tests = \
	10-sqlite-misused.py \
	11-sqlite-batch-misused.py \
	12-transactions.py \
	13-threaded-store.py

tests.xml:
	@if test -h /targets/links/scratchbox.config ; then \
		export SBOX_REDIRECT_IGNORE=/usr/bin/python ; \
	fi;
	./create-tests-xml.py $(standard_tests) > tests.xml

tracker-tests.aegis:
	@if test -h /targets/links/scratchbox.config ; then \
		export SBOX_REDIRECT_IGNORE=/usr/bin/python ; \
	fi;
	./create-tests-aegis.py $(standard_tests) > tracker-tests.aegis

10-sqlite-misused.py: ttl/gen-test-data.stamp
11-sqlite-misused-batch.py: ttl/gen-test-data.stamp
13-busy-store.py: ttl/gen-test-data.stamp

ttl/gen-test-data.stamp:
	cd ttl && $(MAKE) $(AM_MAKEFLAGS)

functional-test: ${standard_tests}
	@if test -h /targets/links/scratchbox.config ; then \
		export SBOX_REDIRECT_IGNORE=/usr/bin/python ; \
		ENV_COMMAND_WRAPPER=meego-run ;	\
	else \
		ENV_COMMAND_WRAPPER="" ; \
	fi ; \
	for testfile in ${standard_tests} ; do \
	  echo "Trying with" $$ENV_COMMAND_WRAPPER $$testfile ; \
	  test ! -x $$testfile  || $$ENV_COMMAND_WRAPPER ./$$testfile > /dev/null; \
	 done

functional-test-slow: ${slow_tests}
	@if test -h /targets/links/scratchbox.config ; then \
		export SBOX_REDIRECT_IGNORE=/usr/bin/python ; \
		ENV_COMMAND_WRAPPER=meego-run ; \
	else \
		ENV_COMMAND_WRAPPER="" ; \
	fi ; \
	@for test in ${slow_tests} ; do \
	  echo "Trying with" $$ENV_COMMAND_WRAPPER $$test ; \
	  test ! -x $$test  || $$ENV_COMMAND_WRAPPER ./$$test > /dev/null; \
	 done

EXTRA_DIST = \
	$(config_SCRIPTS) \
	create-tests-xml.py \
	trackertest

if HAVE_MAEMO
CLEANFILES = \
	tests.xml \
	tracker-tests.aegis
endif
