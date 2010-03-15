# -*- coding: utf-8 -*-

import tools

####################################################################################
def generateVolume(index):
  datasource_uri  = 'urn:nepomuk:datasource:%d' % index

  # save the last uri
  tools.addUri( 'tracker#Volume', datasource_uri )

  # subsitute into template
  volume = tools.getTemplate( 'tracker#Volume' )
  volume = volume.replace( '${datasource_uri}', datasource_uri )

  # save the result
  tools.addResult( 'tracker#Volume', volume )
 