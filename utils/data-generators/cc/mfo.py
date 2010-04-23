# -*- coding: utf-8 -*-

import tools

####################################################################################
def generateFeedChannel(index):
  me = 'mfo#FeedChannel'
  feed_channel_uri         = 'http://feed%d.feed.com/feed%d.rss' % (index % 1000, index)
  feed_channel_title       = 'Feed %d' % index
  feed_channel_description = 'Description %d' % (index % 1000)

  # save the last uri
  tools.addUri( me, feed_channel_uri )

  # subsitute into template
  channel = tools.getTemplate( me )

  # save the result
  tools.addResult( me, channel % locals())

####################################################################################
def generateFeedMessage(index):
  me = 'mfo#FeedMessage'
  feed_message_uri         = 'http://feed%d.feed.com/message%d.html' % (index % 1000, index)
  feed_message_title       = 'Message %d' % index
  feed_message_description = 'Description %d' % (index % 1000)
  feed_message_comment     = 'Comment %d' % index
  feed_message_channel     = tools.getLastUri( 'mfo#FeedChannel' )

  # save the last uri
  tools.addUri( me, feed_message_uri )

  # subsitute into template
  message = tools.getTemplate( me )

  # save the result
  tools.addResult( me, message % locals() )
