#! /usr/bin/python
#
# Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#

import sys, random, datetime
import barnum.gen_data as gen_data
import string

from internals.tools import getPseudoRandomDate

tags = ["cool", "interesting", "favourite", "boring"]

def getRandomTag():
    return "\"" + tags [random.randint(0, len(tags) - 1)] + "\""

def generateCalendarEntry(gen_data, str, random):
    organizerId = random.randint(0, len(previousContacts) - 1)
    #TODO set eventstart to 00 minutes 00 seconds
    eventStart = datetime.datetime.now() + datetime.timedelta(days=random.randint(0, 30), hours=random.randint(-4, 4))
    eventEnd = eventStart + datetime.timedelta(minutes=(random.randint(1, 4) * 30))
    sys.stdout.write('<urn:uuid:' + calendarEntryID + '> a nie:DataObject, ncal:Event, ncal:CalendarDataObject ;\n')
    sys.stdout.write('\tncal:attendee [ncal:involvedContact <urn:uuid:1>] ;\n')
    sys.stdout.write('\tncal:attendee [ncal:involvedContact <urn:uuid:'+previousContacts[organizerId]+'>] ;\n')
    sys.stdout.write('\tncal:transp ncal:transparentTransparency;\n')
    #sys.stdout.write('\tnie:dataSource <http://nepomuk.semanticdesktop.org/datawrapper/aperture/rootUri/calendar> ;\n')
    sys.stdout.write('\tncal:class ncal:publicClassification ;\n')
    sys.stdout.write('\tncal:summary "' + str.replace(gen_data.create_paragraphs(1, 2, 2), "\n", "") + '" ;\n')
    sys.stdout.write('\tncal:dtstart [ncal:dateTime "' + eventStart.isoformat().split('.')[0] + '"];\n')
    sys.stdout.write('\tncal:dtend [ncal:dateTime "' + eventEnd.isoformat().split('.')[0] + '"];\n')
    #TODO Add variance to location
    sys.stdout.write('\tncal:location "Helsinki, Finland" ;\n')
    sys.stdout.write('\tncal:sequence 0 ;\n')
    sys.stdout.write('\tncal:url <http://TODO-fillmehere.com> ;\n')
    sys.stdout.write('\tncal:organizer [ncal:involvedContact <urn:uuid:'+ previousContacts[organizerId]+'>] ;\n')
    sys.stdout.write('\tncal:priority 5 ;\n')
    if (random.randint(0, 4) > 3):
        sys.stdout.write ('\tnao:hasTag [a nao:Tag ; nao:prefLabel ' +  getRandomTag () +'];\n')
    #Wow - hard to create uid?
    sys.stdout.write('\tncal:uid "040000008200E00074C5B7101A82E00800000000B020A967E159C8010000000000000000100000001F009082EE836A4D9E9F85D0FD610DDC" ;\n')
    sys.stdout.write('\tncal:dtstamp "' + datetime.datetime.now().isoformat().split('.')[0] + '" .\n')
    sys.stdout.write('\n')

def generateIMAccount(gen_data, str):
    sys.stdout.write('<xmpp:' + xmppAddress + '> a nco:IMAccount; \n')
    sys.stdout.write('\tnco:imAccountType "jabber" ;\n')
    sys.stdout.write('\tnco:imNickname "' + firstName + ' ' + lastName + '" ;\n')
    #sys.stdout.write('\tnco:imStatus "online" ;\n')
    sys.stdout.write('\tnco:imStatusMessage "' + str.replace(gen_data.create_paragraphs(1, 2, 2), "\n", "") + '" ;\n')
    sys.stdout.write('\tnco:imID "' + xmppAddress + '".\n')
    sys.stdout.write('\n')
    previousIMAccounts.append ('xmpp:' + xmppAddress)

def generatePhoneNumber():
    sys.stdout.write('<' + phoneUri + '> a nco:PhoneNumber; \n')
    sys.stdout.write('\tnco:phoneNumber "' + phoneNumber + '".\n')
    sys.stdout.write('\n')

## def generateImplicitContact (telephoneNumber):
##     objUID = str(random.randint(0, sys.maxint))
##     sys.stdout.write('<urn:uuid:' + objUID +'> a nco:Contact; \n')
##     sys.stdout.write('\tnco:hasPhoneNumber <' + telephoneNumber + '> .\n')
##     sys.stdout.write('\n')
##     return objUID

def generatePhoneCalls (many):
    for i in range (0, many):
        callUID = str(random.randint(0, sys.maxint))

        duration = random.randint (0, 50)
        relationType = random.randint (0,100) % 2
        if (relationType == 0):
            contactProperty = "nmo:from"
            timeProperty = "nmo:receivedDate"
        else:
            contactProperty = "nmo:to"
            timeProperty = "nmo:sentDate"

        sys.stdout.write ('<urn:uuid:' + callUID + '> a nmo:TelephoneCall; \n')
        sys.stdout.write ('\t'+ contactProperty
                 +' [a nco:Contact; '
                 +'nco:hasPhoneNumber ' + '<' + phoneUri + '>]; \n')
        sys.stdout.write ('\tnmo:duration ' + str(duration) + '; \n')
        sys.stdout.write ('\t'+ timeProperty +' "' + getPseudoRandomDate () + '". \n')
        sys.stdout.write ('\n')
        #taggableUris.append ("urn:uuid:" + callUID)

def generateSMS (many):
    for i in range (0, many):
        smsUID = str(random.randint (0, sys.maxint))

        sys.stdout.write ('<urn:uuid:' + smsUID + '> a nmo:SMSMessage ;\n')
        if (random.randint (0,100) % 2 == 0):
            #Sent SMS
            sys.stdout.write('\tnmo:to [a nco:Contact; nco:hasPhoneNumber <' + phoneUri + '>];\n')
            sys.stdout.write('\tnmo:from [a nco:Contact; nco:hasPhoneNumber <' + myOwnPhoneNumberURI + '>];\n')
        else:
            #Received SMS
            sys.stdout.write('\tnmo:from [a nco:Contact; nco:hasPhoneNumber <' + phoneUri + '>];\n')
            sys.stdout.write('\tnmo:to [a nco:Contact; nco:hasPhoneNumber <' + myOwnPhoneNumberURI + '>];\n')

        sys.stdout.write('\tnmo:sentDate "' + getPseudoRandomDate () + '";\n')
        sys.stdout.write('\tnmo:receivedDate "' + getPseudoRandomDate () + '";\n')
        if (random.randint(0, 4) > 3):
            sys.stdout.write ('\tnao:hasTag [a nao:Tag ; nao:prefLabel ' +  getRandomTag () +'];\n')
        sys.stdout.write('\tnmo:plainTextMessageContent "' + str.replace(gen_data.create_paragraphs(1, 5, 8), "\n", "") + '".\n')
        sys.stdout.write('\n')

def generateEmailAddress():
    sys.stdout.write('<mailto:' + emailAddress + '> a nco:EmailAddress; \n')
    sys.stdout.write('\tnco:emailAddress "' + emailAddress + '".\n')
    sys.stdout.write('\n')

def generatePostalAddress():
    sys.stdout.write('<urn:uuid:' + postalAddressID + '> a nco:PostalAddress; \n')
    sys.stdout.write('\tnco:country "US" ;\n')
    sys.stdout.write('\tnco:region "' + state + '" ;\n')
    sys.stdout.write('\tnco:postalcode "' + zip + '" ;\n')
    sys.stdout.write('\tnco:locality "' + city + '" ;\n')
    sys.stdout.write('\tnco:streetAddress "' + streetAddress + '" .\n')
    sys.stdout.write('\n')

def generateEmail(sys, gen_data, str, random):
    emailUID = str(random.randint(0, sys.maxint))
    sentMoment = datetime.datetime.now() - datetime.timedelta(days=random.randint(0, 400), minutes=random.randint(0, 59), seconds=random.randint(0, 59))
    receivedMoment = sentMoment + datetime.timedelta(seconds=random.randint(0, 59))
    sys.stdout.write('<urn:uuid:' + emailUID + '> a nmo:Email; \n')
    sys.stdout.write('\tnmo:to <urn:uuid:1>;\n')
    sys.stdout.write('\tnmo:from <urn:uuid:' + UID + '>;\n')
    sys.stdout.write('\tnmo:sentDate "' + sentMoment.isoformat().split('.')[0] + '";\n')
    sys.stdout.write('\tnmo:receivedDate "' + receivedMoment.isoformat().split('.')[0] + '";\n')
    sys.stdout.write('\tnmo:contentMimeType "text/plain";\n')
    #if random.randint(0, 11)>9 and len(previousContacts)>1: sys.stdout.write('\tnmo:isRead true;\n')
    #else: sys.stdout.write('\tnmo:isRead false;\n')
    sys.stdout.write('\tnmo:messageHeader [a nmo:MessageHeader; nmo:headerName "User-Agent"; nmo:headerValue "tin/unoff-1.3-BETA-970813 (UNIX) (Linux/2.0.30 (i486)) "];\n')
    sys.stdout.write('\tnmo:messageHeader [a nmo:MessageHeader; nmo:headerName "from"; nmo:headerValue "' + emailAddress + '"];\n')
    sys.stdout.write('\tnmo:messageHeader [a nmo:MessageHeader; nmo:headerName "to"; nmo:headerValue "me@me.com"];\n')
    if random.randint(0, 10) > 6 and len(previousContacts) > 1:
        ccid = random.randint(0, len(previousContacts) - 1)
        sys.stdout.write('\tnmo:cc <urn:uuid:' + previousContacts[ccid] + '>;\n')
        sys.stdout.write('\tnmo:messageHeader [a nmo:MessageHeader; nmo:headerName "cc"; nmo:headerValue "' + previousEmailAddresses[ccid] + '"];\n')

    if random.randint(0, 10) > 7 and len(previousContacts) > 1:
        bccid = random.randint(0, len(previousContacts) - 1)
        sys.stdout.write('\tnmo:bcc <urn:uuid:' + previousContacts[bccid] + '>;\n')
        sys.stdout.write('\tnmo:messageHeader [a nmo:MessageHeader; nmo:headerName "bcc"; nmo:headerValue "' + previousEmailAddresses[bccid] + '"];\n')
    #TODO add some sense to the email titles. Some reply chains as well.
    if (random.randint(0, 4) > 3):
        sys.stdout.write ('\tnao:hasTag [a nao:Tag ; nao:prefLabel ' +  getRandomTag () +'];\n')

    sys.stdout.write('\tnmo:messageSubject "' + str.replace(gen_data.create_paragraphs(1, 2, 2), "\n", "") + '";\n')
    sys.stdout.write('\tnmo:plainTextMessageContent "' + str.replace(gen_data.create_paragraphs(1, 2, 3), "\n", "") + '".\n')

# BEGIN CREATING.
if (len(sys.argv) < 2):
    sys.stderr.write("Usage: python get_ttl.py NUMBER_OF_CONTACTS [--with-phone]")
    sys.exit(0)

try:
    count = int(sys.argv[1])
except:
    sys.stderr.write("Usage: python get_ttl.py NUMBER_OF_CONTACTS [--with-phone]")
    sys.exit(0)

if (len(sys.argv) > 2):
    if (sys.argv[2] == "--with-phone") :
        withPhone = True
    else:
        withPhone = False
else:
    withPhone = False

sys.stdout.write("@prefix rdfs:  <http://www.w3.org/2000/01/rdf-schema#>.\n")
sys.stdout.write("@prefix nrl:   <http://www.semanticdesktop.org/ontologies/2007/08/15/nrl#>.\n")
sys.stdout.write("@prefix nid3:   <http://www.semanticdesktop.org/ontologies/2007/05/10/nid3#>.\n")
sys.stdout.write("@prefix nao:   <http://www.semanticdesktop.org/ontologies/2007/08/15/nao#>.\n")
sys.stdout.write("@prefix nco:   <http://www.semanticdesktop.org/ontologies/2007/03/22/nco#>.\n")
sys.stdout.write("@prefix nmo:   <http://www.semanticdesktop.org/ontologies/2007/03/22/nmo#>.\n")
sys.stdout.write("@prefix nfo:   <http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#>.\n")
sys.stdout.write("@prefix nie:   <http://www.semanticdesktop.org/ontologies/2007/01/19/nie#>.\n")
sys.stdout.write("@prefix ncal:   <http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#>.\n")

sys.stdout.write("@prefix xsd:   <http://www.w3.org/2001/XMLSchema#>.\n")
sys.stdout.write('<mailto:me@me.com> a nco:EmailAddress; \n')
sys.stdout.write('\tnco:emailAddress "me@me.com".\n')
sys.stdout.write('\n')
sys.stdout.write('<urn:uuid:1> a nco:PersonContact; \n')
sys.stdout.write('\tnco:fullname "Me Myself";\n')
sys.stdout.write('\tnco:nameGiven "Me";\n')
sys.stdout.write('\tnco:nameFamily "Myself";\n')
sys.stdout.write('\tnco:birthDate "2008-01-01";\n')
sys.stdout.write('\tnco:hasEmailAddress <mailto:me@me.com>;\n')
sys.stdout.write('\tnco:hasPhoneNumber <tel:+11111111111>.\n')
sys.stdout.write('\n')
sys.stdout.write('<tel:+11111111111> a nco:PhoneNumber; \n')
sys.stdout.write('\tnco:phoneNumber "(111) 111-1111".\n')
sys.stdout.write('\n')

#TODO need to create some email folders
myOwnPhoneNumberURI = "tel:+11111111111"
previousContacts = []
previousEmailAddresses = []
previousIMAccounts = []
allchars = string.maketrans('','')

for dummy in range (0, count):
    firstName, lastName = gen_data.create_name()
    zip, city, state = gen_data.create_city_state_zip()
    postalAddressID=str(random.randint(0, sys.maxint))

    UID = str(random.randint(0, sys.maxint))
    phoneNumber = gen_data.create_phone()
    phoneUri = 'tel:+1' + phoneNumber.translate(allchars,' -()')
    birthDay = gen_data.create_birthday()
    streetAddress = gen_data.create_street()
    emailAddress = gen_data.create_email(name=(firstName, lastName))
    xmppAddress = str(firstName+"." + lastName + "@gmail.com").lower()
    hasIMAccount = False
    hasPhoneNumber = False
    jobTitle = gen_data.create_job_title()

    generatePostalAddress()
    generateEmailAddress()

    #Only every 3rd have Phone or IM to add variation.
    if random.randint(0, 3) > 2 or count == 1:
        generateIMAccount(gen_data, str)
        hasIMAccount = True
    if random.randint(0, 3) > 2 or count == 1:
        generatePhoneNumber()
        hasPhoneNumber = True

        if (withPhone): generatePhoneCalls(3)
        if (withPhone): generateSMS (4)

    sys.stdout.write('<urn:uuid:' + UID + '> a nco:PersonContact; \n')
    sys.stdout.write('\tnco:fullname "' + firstName +  ' ' + lastName +'";\n')
    sys.stdout.write('\tnco:nameGiven "' + firstName + '";\n')
    sys.stdout.write('\tnco:nameFamily "' + lastName + '";\n')
    sys.stdout.write('\tnco:birthDate "' + str(birthDay) + '";\n')
    #sys.stdout.write('\tnco:title "'+jobTitle+'";\n')
    sys.stdout.write('\tnco:hasEmailAddress <mailto:' + emailAddress + '>;\n')
    if hasPhoneNumber: sys.stdout.write('\tnco:hasPhoneNumber <' + phoneUri + '>;\n')
    if hasIMAccount: sys.stdout.write('\tnco:hasIMAccount <xmpp:' + xmppAddress + '>;\n')
    if (random.randint(0, 4) > 3):
        sys.stdout.write ('\tnao:hasTag [a nao:Tag ; nao:prefLabel ' + getRandomTag () + '];\n')
    sys.stdout.write('\tnco:hasPostalAddress <urn:uuid:' + postalAddressID + '>.\n')
    sys.stdout.write('\n')

    #calendarEntryID=str(random.randint(0, sys.maxint))
    #if random.randint(0, 3)>2 and count>2 and len(previousContacts):
    #    generateCalendarEntry(gen_data, str, random)

    #20% Send emails. Those who do, send 1-30 emails. EMails have CC and BCC people
    if random.randint(0, 10)>8 or count==1:
        emailcount=random.randint(1, 30)
        for dummy in range (0, emailcount):
            generateEmail(sys, gen_data, str, random)
            sys.stdout.write('\n')
    previousContacts.append(UID)
    previousEmailAddresses.append(emailAddress)

    #TODO INSERT IM - Use just a nmo:Message for that for now.

    #TODO: Insert bookmarks

