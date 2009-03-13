include $(top_srcdir)/Makefile.decl

SUBDIRS = .

if USING_RSS_PUSH_MODULE
SUBDIRS += rss
endif

if USING_KMAIL_PUSH_MODULE
SUBDIRS += kmail
endif

if USING_EVOLUTION_PUSH_MODULE
if HAVE_EVOLUTION_PLUGIN
SUBDIRS += evolution
endif
endif

