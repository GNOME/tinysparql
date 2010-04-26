# -*- coding: utf-8 -*-

import tools

####################################################################################
tracker_Volume = '''
<%(datasource_uri)s> a tracker:Volume .
'''
def generateVolume(index):
  me = 'tracker#Volume'
  datasource_uri  = 'urn:nepomuk:datasource:%d' % index

  tools.addItem( me, datasource_uri, tracker_Volume % locals() )
