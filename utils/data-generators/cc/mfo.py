# -*- coding: utf-8 -*-

import tools

####################################################################################
mfo_FeedChannel = '''
<%(feed_channel_uri)s> a mfo:FeedChannel ;
  nie:title       "%(feed_channel_title)s" ;
  nie:relatedTo   <%(feed_channel_uri)s> ;
  nie:description "%(feed_channel_description)s" ;
  nie:links       <%(feed_channel_uri)s> ;
  mfo:image       "" ;
  mfo:type        <http://www.tracker-project.org/temp/mfo#rssatom> .
'''
def generateFeedChannel(index):
  me = 'mfo#FeedChannel'
  feed_channel_uri         = 'http://feed%d.feed.com/feed%d.rss' % (index % 1000, index)
  feed_channel_title       = 'Feed %d' % index
  feed_channel_description = 'Description %d' % (index % 1000)

  tools.addItem( me, feed_channel_uri, mfo_FeedChannel % locals() );

####################################################################################
mfo_FeedMessage = '''
<%(feed_message_uri)s> a mfo:FeedMessage ;
  nie:title                "%(feed_message_title)s" ;
  nie:relatedTo            <%(feed_message_uri)s> ;
  nie:description          "%(feed_message_description)s" ;
  nie:links                <%(feed_message_uri)s> ;
  nie:comment              "%(feed_message_comment)s" ;
  nmo:communicationChannel <%(feed_message_channel)s> .
'''
def generateFeedMessage(index):
  me = 'mfo#FeedMessage'
  feed_message_uri         = 'http://feed%d.feed.com/message%d.html' % (index % 1000, index)
  feed_message_title       = 'Message %d' % index
  feed_message_description = 'Description %d' % (index % 1000)
  feed_message_comment     = 'Comment %d' % index
  feed_message_channel     = tools.getLastUri( 'mfo#FeedChannel' )

  tools.addItem( me, feed_message_uri, mfo_FeedMessage % locals() )
