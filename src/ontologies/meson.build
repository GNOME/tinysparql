base_ontology = files(
    '10-xsd.description',
    '10-xsd.ontology',
    '11-rdf.description',
    '11-rdfs.description',
    '11-rdf.ontology',
    '12-nrl.description',
    '12-nrl.ontology',
    '20-dc.description',
    '20-dc.ontology',
)

nepomuk = files(
    'nepomuk/30-nie.description',
    'nepomuk/30-nie.ontology',
    'nepomuk/31-nao.description',
    'nepomuk/31-nao.ontology',
    'nepomuk/32-nco.description',
    'nepomuk/32-nco.ontology',
    'nepomuk/33-nfo.description',
    'nepomuk/33-nfo.ontology',
    'nepomuk/38-nmm.description',
    'nepomuk/38-nmm.ontology',
    'nepomuk/41-mfo.description',
    'nepomuk/41-mfo.ontology',
    'nepomuk/90-tracker.description',
    'nepomuk/90-tracker.ontology',
    'nepomuk/92-slo.description',
    'nepomuk/92-slo.ontology',
    'nepomuk/93-libosinfo.description',
    'nepomuk/93-libosinfo.ontology',
)

ontology_incs = include_directories('.')

tracker_gresources = gnome.compile_resources(
    'tracker-gresources',
    'builtin-ontology.gresource.xml',
    extra_args: ['--manual-register'])

tracker_nepomuk_gresources = gnome.compile_resources(
    'tracker-nepomuk-gresources',
    'nepomuk-ontology.gresource.xml',
    extra_args: ['--manual-register'])
