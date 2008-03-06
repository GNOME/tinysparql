INCLUDES =				\
	$(GLIB2_CFLAGS)			\
	-DSHAREDIR=\""$(datadir)"\"

noinst_LTLIBRARIES = libtracker-common.la

libtracker_common_la_SOURCES = 		\
	tracker-configuration.c 	\
	tracker-configuration.h 	\
	tracker-config.c 		\
	tracker-config.h		\
	tracker-language.c		\
	tracker-language.h		\
	tracker-log.c	 		\
	tracker-log.h

libtracker_common_la_LDFLAGS = -version-info 0:0:0
libtracker_common_la_LIBADD = $(GLIB2_LIBS)

noinst_HEADERS = 			\
	tracker-configuration.h		\
	tracker-config.h		\
	tracker-language.h		\
	tracker-log.h
