include $(top_srcdir)/Makefile.decl

INCLUDES =				\
	$(BUILD_CFLAGS)			\
	-I$(top_srcdir)/src		\
	-DSHAREDIR=\""$(datadir)"\"

noinst_LTLIBRARIES = libstemmer.la

libstemmer_la_SOURCES = 		\
	libstemmer.c 			\
	api.c 				\
	utilities.c 			\
	stem_UTF_8_danish.c 		\
	stem_UTF_8_dutch.c 		\
	stem_UTF_8_english.c 		\
	stem_UTF_8_finnish.c 		\
	stem_UTF_8_french.c 		\
	stem_UTF_8_german.c 		\
	stem_UTF_8_hungarian.c 		\
	stem_UTF_8_italian.c 		\
	stem_UTF_8_norwegian.c 		\
	stem_UTF_8_porter.c 		\
	stem_UTF_8_portuguese.c 	\
	stem_UTF_8_russian.c 		\
	stem_UTF_8_spanish.c 		\
	stem_UTF_8_swedish.c

noinst_HEADERS =			\
	api.h				\
	header.h			\
	libstemmer.h			\
	modules.h			\
	stem_UTF_8_danish.h		\
	stem_UTF_8_dutch.h		\
	stem_UTF_8_english.h		\
	stem_UTF_8_finnish.h		\
	stem_UTF_8_french.h		\
	stem_UTF_8_german.h		\
	stem_UTF_8_hungarian.h		\
	stem_UTF_8_italian.h		\
	stem_UTF_8_norwegian.h		\
	stem_UTF_8_porter.h		\
	stem_UTF_8_portuguese.h		\
	stem_UTF_8_russian.h		\
	stem_UTF_8_spanish.h		\
	stem_UTF_8_swedish.h


libstemmer_la_LIBADD = $(BUILD_LIBS)

