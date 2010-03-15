# -*- coding: utf-8 -*-

import tools

####################################################################################
def generateFeedChannel(index):
  feed_channel_uri         = 'http://feed%d.feed.com/feed%d.rss' % (index % 1000, index % 1000)
  feed_channel_title       = 'Feed %d' % index
  feed_channel_description = 'Description %d' % (index % 1000)

  # save the last uri
  tools.addUri( 'mfo#FeedChannel', feed_channel_uri )

  # subsitute into template
  channel = tools.getTemplate( 'mfo#FeedChannel' )
  channel = channel.replace( '${feed_channel_uri}', feed_channel_uri )
  channel = channel.replace( '${feed_channel_title}', feed_channel_title )
  channel = channel.replace( '${feed_channel_description}', feed_channel_description )

  # save the result
  tools.addResult( 'mfo#FeedChannel', channel )

####################################################################################
def generateFeedMessage(index):
  feed_message_uri         = 'http://feed%d.feed.com/message%d.html' % (index % 1000, index % 1000)
  feed_message_title       = 'Message %d' % index
  feed_message_description = 'Description %d' % (index % 1000)
  feed_message_comment     = 'Comment %d' % index
  feed_message_channel     = tools.getLastUri( 'mfo#FeedChannel' )

  # save the last uri
  tools.addUri( 'mfo#FeedMessage', feed_message_uri )

  # subsitute into template
  message = tools.getTemplate( 'mfo#FeedMessage' )
  message = message.replace( '${feed_message_uri}', feed_message_uri )
  message = message.replace( '${feed_message_title}', feed_message_title )
  message = message.replace( '${feed_message_description}', feed_message_description )
  message = message.replace( '${feed_message_comment}', feed_message_comment )
  message = message.replace( '${feed_message_channel}', feed_message_channel )

  # save the result
  tools.addResult( 'mfo#FeedMessage', message )
