# -*- coding: utf-8 -*-

import tools

####################################################################################
def generateAlarm(index):
  me = 'ncal#Alarm'
  alarm_uri          = 'urn:x-ical:alarm%d' % index
  alarm_repeat       = '%d' % ( 3600 *  ((index % 10) + 1) )
  alarm_duration     = '%d' % ( 1 + (index % 10) )
  alarm_trigger_date = '%d-%02d-%02dT%02d:%02d:%02dZ' % (1900 + (index % 100), (index % 12) + 1, (index % 25) + 1, (index % 12) + 1, (index % 12) + 1, (index % 12) + 1)
  alarm_subject      = 'Subject %d' % index
  alarm_description  = 'Description %d' % index

  # save the last uri
  tools.addUri( me, alarm_uri )

  # subsitute into template
  alarm = tools.getTemplate( me )

  # save the result
  tools.addResult( me, alarm % locals() )

####################################################################################
def generateCalendar(index):
  me = 'ncal#Calendar'
  calendar_uri = 'urn:x-ical:calendar%d' % index

  # save the last uri
  tools.addUri( me, calendar_uri )

  # subsitute into template
  calendar = tools.getTemplate( me )

  # save the result
  tools.addResult( me, calendar % locals() )


####################################################################################
def generateEvent(index):
  me = 'ncal#Event'
  event_uri      = 'urn:x-ical:%d' % index
  event_uid      = '%d' % index
  event_start    = '%d-%02d-%02dT09:00:00Z' % (2010 + (index % 5), (index % 12) + 1, (index % 25) + 1)
  event_end      = '%d-%02d-%02dT17:00:00Z' % (2010 + (index % 5), (index % 12) + 1, (index % 25) + 1)
  event_summary  = 'Event %d' % index
  event_created  = tools.getDateNowString()
  event_modified = event_created
  alarm_uri      = tools.getLastUri( 'ncal#Alarm' )
  calendar_uri   = tools.getRandomUri( 'ncal#Calendar' )

  # save the last uri
  tools.addUri( me, event_uri )

  # subsitute into template
  event = tools.getTemplate( me )

  # save the result
  tools.addResult( me, event % locals() )

####################################################################################
def generateTodo(index):
  me = 'ncal#Todo'
  todo_uri      = 'urn:todo::%d' % index
  todo_uid      = '%d' % index
  todo_summary  = 'Todo %d' % index
  todo_created  = tools.getDateNowString()
  todo_modified = todo_created
  alarm_uri     = tools.getRandomUri( 'ncal#Alarm' )
  calendar_uri  = tools.getRandomUri( 'ncal#Calendar' )

  # save the last uri
  tools.addUri( me, todo_uri )

  # subsitute into template
  todo = tools.getTemplate( me )

  # save the result
  tools.addResult( me, todo % locals() )
