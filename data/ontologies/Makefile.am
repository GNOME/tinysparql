include $(top_srcdir)/Makefile.decl

maemo_ontology =                                       \
	91-maemo.ontology   91-maemo.description

configdir = $(datadir)/tracker/ontologies

config_DATA =                                          \
	10-xsd.ontology	    10-xsd.description         \
	11-rdf.ontology	    11-rdf.description         \
	12-nrl.ontology	    12-nrl.description         \
	20-dc.ontology      20-dc.description          \
	30-nie.ontology	    30-nie.description         \
	31-nao.ontology	    31-nao.description         \
	32-nco.ontology     32-nco.description         \
	33-nfo.ontology	    33-nfo.description         \
	34-nmo.ontology	    34-nmo.description         \
	35-ncal.ontology    35-ncal.description        \
	36-scal.ontology    36-scal.description        \
	37-nid3.ontology    37-nid3.description        \
	38-nmm.ontology	    38-nmm.description         \
	39-mto.ontology	    39-mto.description         \
	40-mlo.ontology	    40-mlo.description         \
	41-mfo.ontology	    41-mfo.description         \
	89-mtp.ontology	    89-mtp.description         \
	92-slo.ontology     92-slo.description         \
	90-tracker.ontology 90-tracker.description

if HAVE_MAEMO
config_DATA +=                                         \
	$(maemo_ontology)
endif

EXTRA_DIST =                                           \
	$(maemo_ontology)                              \
	$(config_DATA)

