include $(top_srcdir)/Makefile.decl

SUBDIRS = data

if DIST_FUNCTIONAL_TESTS
configdir = $(datadir)/tracker-tests

config_DATA =			\
	tests.xml		\
	01-insertion.py		\
	02-metacontacts.py	\
	03-fts-functions.py	\
	configuration.py	\
	metadata_extraction_tc.py \
	04-group-concat.py	\
	05-coalesce.py

EXTRA_DIST = $(config_DATA)
endif
