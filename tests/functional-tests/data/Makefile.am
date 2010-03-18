include $(top_srcdir)/Makefile.decl

SUBDIRS = 		\
	Music		\
	Images

if DIST_FUNCTIONAL_TESTS
configdir = $(datadir)/tracker-tests/data

config_DATA =			\
	pickled_Music		\
	pickled_Images

EXTRA_DIST = $(config_DATA)
endif
