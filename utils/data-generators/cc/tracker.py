# -*- coding: utf-8 -*-

import tools

####################################################################################
def generateVolume(index):
  me = 'tracker#Volume'
  datasource_uri  = 'urn:nepomuk:datasource:%d' % index

  # save the last uri
  tools.addUri( me, datasource_uri )

  # subsitute into template
  volume = tools.getTemplate( 'tracker#Volume' )
  tools.addResult(me, volume % locals())
