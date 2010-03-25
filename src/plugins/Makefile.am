include $(top_srcdir)/Makefile.decl

SUBDIRS = 

if USING_KMAIL_MINER
SUBDIRS += kmail
endif

if USING_EVOLUTION_MINER
if HAVE_EVOLUTION_PLUGIN
SUBDIRS += evolution
endif
endif

if HAVE_NAUTILUS_EXTENSION
SUBDIRS += nautilus
endif
