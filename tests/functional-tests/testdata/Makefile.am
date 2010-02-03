include $(top_srcdir)/Makefile.decl

SUBDIRS = 		\
	ttl		\
	Music

if DIST_FUNCTIONAL_TESTS
configdir = $(datadir)/tracker-tests/data

config_DATA =			\
	pickled_Music

EXTRA_DIST = $(config_DATA)
endif
