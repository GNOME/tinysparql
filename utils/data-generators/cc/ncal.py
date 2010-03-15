# -*- coding: utf-8 -*-

import tools

####################################################################################
def generateAlarm(index):
  alarm_uri          = 'urn:x-ical:alarm%d' % index
  alarm_repeat       = '%d' % ( 3600 *  ((index % 10) + 1) )
  alarm_duration     = '%d' % ( 1 + (index % 10) )
  alarm_trigger_date = '%d-%02d-%02dT%02d:%02d:%02dZ' % (1900 + (index % 100), (index % 12) + 1, (index % 25) + 1, (index % 12) + 1, (index % 12) + 1, (index % 12) + 1)
  alarm_subject      = 'Subject %d' % index
  alarm_description  = 'Description %d' % index

  # save the last uri
  tools.addUri( 'ncal#Alarm', alarm_uri )

  # subsitute into template
  alarm = tools.getTemplate( 'ncal#Alarm' )
  alarm = alarm.replace( '${alarm_uri}', alarm_uri )
  alarm = alarm.replace( '${alarm_repeat}', alarm_repeat )
  alarm = alarm.replace( '${alarm_duration}', alarm_duration )
  alarm = alarm.replace( '${alarm_trigger_date}', alarm_trigger_date )
  alarm = alarm.replace( '${alarm_subject}', alarm_subject )
  alarm = alarm.replace( '${alarm_description}', alarm_description )

  # save the result
  tools.addResult( 'ncal#Alarm', alarm )

####################################################################################
def generateCalendar(index):
  calendar_uri = 'urn:x-ical:calendar%d' % index

  # save the last uri
  tools.addUri( 'ncal#Calendar', calendar_uri )

  # subsitute into template
  calendar = tools.getTemplate( 'ncal#Calendar' )
  calendar = calendar.replace( '${calendar_uri}', calendar_uri )

  # save the result
  tools.addResult( 'ncal#Calendar', calendar )


####################################################################################
def generateEvent(index):
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
  tools.addUri( 'ncal#Event', event_uri )

  # subsitute into template
  event = tools.getTemplate( 'ncal#Event' )
  event = event.replace( '${event_uri}', event_uri )
  event = event.replace( '${event_uid}', event_uid )
  event = event.replace( '${event_start}', event_start )
  event = event.replace( '${event_end}', event_end )
  event = event.replace( '${event_summary}', event_summary )
  event = event.replace( '${event_created}', event_created )
  event = event.replace( '${event_modified}', event_modified )
  event = event.replace( '${alarm_uri}', alarm_uri )
  event = event.replace( '${calendar_uri}', calendar_uri )

  # save the result
  tools.addResult( 'ncal#Event', event )

####################################################################################
def generateTodo(index):
  todo_uri      = 'urn:todo::%d' % index
  todo_uid      = '%d' % index
  todo_summary  = 'Todo %d' % index
  todo_created  = tools.getDateNowString()
  todo_modified = todo_created
  alarm_uri     = tools.getRandomUri( 'ncal#Alarm' )
  calendar_uri  = tools.getRandomUri( 'ncal#Calendar' )

  # save the last uri
  tools.addUri( 'ncal#Todo', todo_uri )

  # subsitute into template
  todo = tools.getTemplate( 'ncal#Todo' )
  todo = todo.replace( '${todo_uri}', todo_uri )
  todo = todo.replace( '${todo_uid}', todo_uid )
  todo = todo.replace( '${todo_summary}', todo_summary )
  todo = todo.replace( '${todo_created}', todo_created )
  todo = todo.replace( '${todo_modified}', todo_modified )
  todo = todo.replace( '${alarm_uri}', alarm_uri )
  todo = todo.replace( '${calendar_uri}', calendar_uri )

  # save the result
  tools.addResult( 'ncal#Todo', todo )
