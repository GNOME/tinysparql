# General
localesdir = $(datadir)/xul-ext/trackerbird/chrome/locale/

localesdata = \
	options.dtd \
	about.dtd \
	overlay.dtd \
	overlay.properties

# locales = de-DE en-US es-ES fr-FR pl-PL

# de-DE
locale_de_DEdir = $(localesdir)/de-DE/
locale_de_DE_DATA = $(patsubst %,de-DE/%,$(localesdata))

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
	$(locale_de_DE_DATA) \
	$(locale_en_US_DATA) \
	$(locale_es_ES_DATA) \
	$(locale_fr_FR_DATA) \
	$(locale_pl_PL_DATA)
