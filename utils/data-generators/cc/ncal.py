# -*- coding: utf-8 -*-

import tools

####################################################################################
ncal_Alarm = '''
<%(alarm_uri)s> a ncal:Alarm;
    ncal:action      ncal:emailAction;
    ncal:repeat      "%(alarm_repeat)s";
    ncal:duration    "%(alarm_duration)s";
    ncal:trigger [ a ncal:Trigger; ncal:triggerDateTime "%(alarm_trigger_date)s" ];
    ncal:summary     "%(alarm_subject)s";
    ncal:description "%(alarm_description)s" .
'''
def generateAlarm(index):
  me = 'ncal#Alarm'
  alarm_uri          = 'urn:x-ical:alarm%d' % index
  alarm_repeat       = '%d' % ( 3600 *  ((index % 10) + 1) )
  alarm_duration     = '%d' % ( 1 + (index % 10) )
  alarm_trigger_date = '%d-%02d-%02dT%02d:%02d:%02dZ' % (1900 + (index % 100), (index % 12) + 1, (index % 25) + 1, (index % 12) + 1, (index % 12) + 1, (index % 12) + 1)
  alarm_subject      = 'Subject %d' % index
  alarm_description  = 'Description %d' % index

  tools.addItem( me, alarm_uri, ncal_Alarm % locals() )

####################################################################################
ncal_Calendar = '''
<%(calendar_uri)s> a ncal:Calendar .
'''
def generateCalendar(index):
  me = 'ncal#Calendar'
  calendar_uri = 'urn:x-ical:calendar%d' % index

  # save the last uri
  tools.addItem( me, calendar_uri, ncal_Calendar % locals() )

####################################################################################
ncal_Event = '''
<%(event_uri)s> a ncal:Event, nie:DataObject;
  ncal:uid      "%(event_uid)s";
  ncal:dtstart [ a ncal:NcalDateTime;
    ncal:dateTime "%(event_start)s";
    ncal:ncalTimezone <urn:x-ical:timezone:Europe/Helsinki> ];
  ncal:dtend [ a ncal:NcalDateTime;
    ncal:dateTime "%(event_end)s";
    ncal:ncalTimezone <urn:x-ical:timezone:Europe/Helsinki> ];
  ncal:transp        ncal:opaqueTransparency;
  ncal:summary       "%(event_summary)s";
  ncal:class         ncal:publicClassification;
  ncal:eventStatus   ncal:;
  ncal:priority      0;
  ncal:dtstamp       "%(event_created)s";
  ncal:created       "%(event_created)s";
  ncal:lastModified  "%(event_modified)s";
  ncal:sequence       0;
  ncal:url            <%(event_uri)s>;
  ncal:hasAlarm       <%(alarm_uri)s> ;
  nie:isLogicalPartOf <%(calendar_uri)s> .
'''
def generateEvent(index):
  me = 'ncal#Event'
  event_uri      = 'urn:x-ical:%d' % index
  event_uid      = '%d' % index
  event_start    = '%d-%02d-%02dT09:00:00Z' % (2010 + (index % 5), (index % 12) + 1, (index % 25) + 1)
  event_end      = '%d-%02d-%02dT17:00:00Z' % (2010 + (index % 5), (index % 12) + 1, (index % 25) + 1)
  event_summary  = 'Event %d' % index
  event_created  = tools.now
  event_modified = event_created
  alarm_uri      = tools.getLastUri( 'ncal#Alarm' )
  calendar_uri   = tools.getRandomUri( 'ncal#Calendar' )

  tools.addItem( me, event_uri, ncal_Event % locals() )

####################################################################################
ncal_Todo = '''
<%(todo_uri)s> a ncal:Todo, nie:DataObject;
  ncal:uid           "%(todo_uid)s";
  ncal:percentComplete 0;
  ncal:summary       "%(todo_summary)s";
  ncal:class         ncal:publicClassification;
  ncal:todoStatus    ncal:;
  ncal:priority      0;
  ncal:dtstamp       "%(todo_created)s";
  ncal:created       "%(todo_created)s";
  ncal:lastModified  "%(todo_modified)s";
  ncal:sequence       0;
  ncal:url            <%(todo_uri)s>;
  ncal:hasAlarm       <%(alarm_uri)s> ;
  nie:isLogicalPartOf <%(calendar_uri)s> .
'''
def generateTodo(index):
  me = 'ncal#Todo'
  todo_uri      = 'urn:todo::%d' % index
  todo_uid      = '%d' % index
  todo_summary  = 'Todo %d' % index
  todo_created  = tools.now
  todo_modified = todo_created
  alarm_uri     = tools.getRandomUri( 'ncal#Alarm' )
  calendar_uri  = tools.getRandomUri( 'ncal#Calendar' )

  tools.addItem( me, todo_uri, ncal_Todo % locals() )
