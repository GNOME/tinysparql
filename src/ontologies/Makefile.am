nepomukdir = $(datadir)/tracker/ontologies/nepomuk

internal_config = \
	10-xsd.ontology	    10-xsd.description         \
	11-rdf.ontology	    11-rdf.description         \
	12-nrl.ontology	    12-nrl.description         \
	20-dc.ontology      20-dc.description          \
	31-nao.ontology	    31-nao.description

nepomuk_DATA =                                          \
	nepomuk/30-nie.ontology	    nepomuk/30-nie.description         \
	nepomuk/32-nco.ontology     nepomuk/32-nco.description         \
	nepomuk/33-nfo.ontology	    nepomuk/33-nfo.description         \
	nepomuk/34-nmo.ontology	    nepomuk/34-nmo.description         \
	nepomuk/35-ncal.ontology    nepomuk/35-ncal.description        \
	nepomuk/36-scal.ontology    nepomuk/36-scal.description        \
	nepomuk/37-nid3.ontology    nepomuk/37-nid3.description        \
	nepomuk/38-nmm.ontology	    nepomuk/38-nmm.description         \
	nepomuk/39-mto.ontology	    nepomuk/39-mto.description         \
	nepomuk/40-mlo.ontology	    nepomuk/40-mlo.description         \
	nepomuk/41-mfo.ontology	    nepomuk/41-mfo.description         \
	nepomuk/89-mtp.ontology	    nepomuk/89-mtp.description         \
	nepomuk/92-slo.ontology     nepomuk/92-slo.description         \
	nepomuk/90-tracker.ontology nepomuk/90-tracker.description     \
	nepomuk/91-maemo.ontology   nepomuk/91-maemo.description       \
	nepomuk/93-libosinfo.ontology nepomuk/93-libosinfo.description

# GResource
tracker_gresource_files = $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/builtin-ontology.gresource.xml)
tracker-gresources.h: builtin-ontology.gresource.xml $(tracker_gresource_files) $(internal_config)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --sourcedir=$(builddir) --generate --c-name tracker_gresources $<
tracker-gresources.c: builtin-ontology.gresource.xml $(tracker_gresource_files) $(internal_config)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --sourcedir=$(builddir) --generate --c-name tracker_gresources $<

BUILT_SOURCES = tracker-gresources.c tracker-gresources.h

EXTRA_DIST = \
	$(internal_config) \
	builtin-ontology.gresource.xml \
	$(nepomuk_DATA)
