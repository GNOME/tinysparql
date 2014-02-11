# General
localesdir = $(datadir)/xul-ext/trackerfox/chrome/locale/

localesdata = \
	about.dtd \
	overlay.dtd \
	overlay.properties

# locales = en-US es-ES fr-FR pl-PL

# en-US
locale_en_USdir = $(localesdir)/en-US/
locale_en_US_DATA = $(patsubst %,en-US/%,$(localesdata))

# es-ES
locale_es_ESdir = $(localesdir)/es-ES/
locale_es_ES_DATA = $(patsubst %,es-ES/%,$(localesdata))

# fr-FR
locale_fr_FRdir = $(localesdir)/fr-FR/
locale_fr_FR_DATA = $(patsubst %,fr-FR/%,$(localesdata))

# pl-PL
locale_pl_PLdir = $(localesdir)/pl-PL/
locale_pl_PL_DATA = $(patsubst %,pl-PL/%,$(localesdata))

EXTRA_DIST = \
	$(locale_en_US_DATA) \
	$(locale_es_ES_DATA) \
	$(locale_fr_FR_DATA) \
	$(locale_pl_PL_DATA)
