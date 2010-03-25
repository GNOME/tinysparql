include $(top_srcdir)/Makefile.decl

SUBDIRS = data

if DIST_FUNCTIONAL_TESTS
configdir = $(datadir)/tracker-tests

config_SCRIPTS =		\
	tests.xml		\
	01-insertion.py		\
	02-metacontacts.py	\
	03-fts-functions.py	\
	configuration.py	\
	metadata_extraction_tc.py \
	writeback-tc.py 	\
	04-group-concat.py	\
	05-coalesce.py		\
	06-distance.py		\
	07-graph.py		\
	08-unique-insertions.py \
	virtual-files-tc.py	\
	performance-tc.py

EXTRA_DIST = $(config_SCRIPTS)
endif
