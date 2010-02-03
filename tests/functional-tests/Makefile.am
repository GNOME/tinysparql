include $(top_srcdir)/Makefile.decl

SUBDIRS = testdata

if DIST_FUNCTIONAL_TESTS
configdir = $(datadir)/tracker-tests

config_DATA =			\
	tests.xml		\
	01-insertion.py		\
	02-metacontacts.py	\
	03-fts-functions.py	\
	configuration.py	\
	metadata_tc.py


EXTRA_DIST = $(config_DATA)
endif
