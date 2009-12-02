#! /usr/bin/python

import sys, random, datetime
import barnum.gen_data as gen_data
import string

from internals.tools import getPseudoRandomDate

tags = ["cool", "interesting", "favourite", "boring"]

def getRandomTag ():
    return "\"" + tags [random.randint(0, len(tags) - 1)] + "\""



def generateCalendarEntry(gen_data, str, random):
    organizerId = random.randint(0, len(previousContacts) - 1)
    #TODO set eventstart to 00 minutes 00 seconds
    eventStart = datetime.datetime.now() + datetime.timedelta(days=random.randint(0, 30), hours=random.randint(-4, 4))
    eventEnd = eventStart + datetime.timedelta(minutes=(random.randint(1, 4) * 30))
    f.write('<urn:uuid:' + calendarEntryID + '> a nie:DataObject, ncal:Event, ncal:CalendarDataObject ;\n')
    f.write('\tncal:attendee [ncal:involvedContact <urn:uuid:1>] ;\n')
    f.write('\tncal:attendee [ncal:involvedContact <urn:uuid:'+previousContacts[organizerId]+'>] ;\n')
    f.write('\tncal:transp ncal:transparentTransparency;\n')
    #f.write('\tnie:dataSource <http://nepomuk.semanticdesktop.org/datawrapper/aperture/rootUri/calendar> ;\n')
    f.write('\tncal:class ncal:publicClassification ;\n')
    f.write('\tncal:summary "' + str.replace(gen_data.create_paragraphs(1, 2, 2), "\n", "") + '" ;\n')
    f.write('\tncal:dtstart [ncal:dateTime "' + eventStart.isoformat().split('.')[0] + '"];\n')
    f.write('\tncal:dtend [ncal:dateTime "' + eventEnd.isoformat().split('.')[0] + '"];\n')
    #TODO Add variance to location
    f.write('\tncal:location "Helsinki, Finland" ;\n')
    f.write('\tncal:sequence 0 ;\n')
    f.write('\tncal:url <http://TODO-fillmehere.com> ;\n')
    f.write('\tncal:organizer [ncal:involvedContact <urn:uuid:'+previousContacts[organizerId]+'>] ;\n')
    f.write('\tncal:priority 5 ;\n')
    if (random.randint(0, 4) > 3):
        f.write ('\tnao:hasTag [a nao:Tag ; nao:prefLabel ' +  getRandomTag () +'];\n')
    #Wow - hard to create uid?
    f.write('\tncal:uid "040000008200E00074C5B7101A82E00800000000B020A967E159C8010000000000000000100000001F009082EE836A4D9E9F85D0FD610DDC" ;\n')
    f.write('\tncal:dtstamp "' + datetime.datetime.now().isoformat().split('.')[0] + '" .\n')
    f.write('\n')

def generateIMAccount(gen_data, str):
    f.write('<xmpp:' + xmppAddress + '> a nco:IMAccount; \n')
    f.write('\tnco:imAccountType "jabber" ;\n')
    f.write('\tnco:imNickname "' + firstName + ' ' + lastName + '" ;\n')
    #f.write('\tnco:imStatus "online" ;\n')
    f.write('\tnco:imStatusMessage "' + str.replace(gen_data.create_paragraphs(1, 2, 2), "\n", "") + '" ;\n')
    f.write('\tnco:imID "' + xmppAddress + '".\n')
    f.write('\n')
    previousIMAccounts.append ('xmpp:' + xmppAddress)


def generatePhoneNumber():
    f.write('<' + phoneUri + '> a nco:PhoneNumber; \n')
    f.write('\tnco:phoneNumber "' + phoneNumber + '".\n')
    f.write('\n')

## def generateImplicitContact (telephoneNumber):
##     objUID = str(random.randint(0, sys.maxint))
##     f.write('<urn:uuid:' + objUID +'> a nco:Contact; \n')
##     f.write('\tnco:hasPhoneNumber <' + telephoneNumber + '> .\n')
##     f.write('\n')
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

        f.write ('<urn:uuid:' + callUID + '> a nmo:TelephoneCall; \n')
        f.write ('\t'+ contactProperty
                 +' [a nco:Contact; '
                 +'nco:hasPhoneNumber ' + '<' + phoneUri + '>]; \n')
        f.write ('\tnmo:duration ' + str(duration) + '; \n')
        f.write ('\t'+ timeProperty +' "' + getPseudoRandomDate () + '". \n')
        f.write ('\n')
        #taggableUris.append ("urn:uuid:" + callUID)

def generateSMS (many):

    for i in range (0, many):

        smsUID = str(random.randint (0, sys.maxint))

        f.write ('<urn:uuid:' + smsUID + '> a nmo:SMSMessage ;\n')
        if (random.randint (0,100) % 2 == 0):
            #Sent SMS
            f.write('\tnmo:to [a nco:Contact; nco:hasPhoneNumber <' + phoneUri + '>];\n')
            f.write('\tnmo:from [a nco:Contact; nco:hasPhoneNumber <' + myOwnPhoneNumberURI + '>];\n')
        else:
            #Received SMS
            f.write('\tnmo:from [a nco:Contact; nco:hasPhoneNumber <' + phoneUri + '>];\n')
            f.write('\tnmo:to [a nco:Contact; nco:hasPhoneNumber <' + myOwnPhoneNumberURI + '>];\n')
            
        f.write('\tnmo:sentDate "' + getPseudoRandomDate () + '";\n')
        f.write('\tnmo:receivedDate "' + getPseudoRandomDate () + '";\n') 
        if (random.randint(0, 4) > 3):
            f.write ('\tnao:hasTag [a nao:Tag ; nao:prefLabel ' +  getRandomTag () +'];\n')
        f.write('\tnmo:plainTextMessageContent "' + str.replace(gen_data.create_paragraphs(1, 5, 8), "\n", "") + '".\n')
        f.write('\n')


def generateEmailAddress():
    f.write('<mailto:' + emailAddress + '> a nco:EmailAddress; \n')
    f.write('\tnco:emailAddress "' + emailAddress + '".\n')
    f.write('\n')

def generatePostalAddress():
    f.write('<urn:uuid:' + postalAddressID + '> a nco:PostalAddress; \n')
    f.write('\tnco:country "US" ;\n')
    f.write('\tnco:region "' + state + '" ;\n')
    f.write('\tnco:postalcode "' + zip + '" ;\n')
    f.write('\tnco:locality "' + city + '" ;\n')
    f.write('\tnco:streetAddress "' + streetAddress + '" .\n')
    f.write('\n')

def generateEmail(sys, gen_data, str, random):
    emailUID = str(random.randint(0, sys.maxint))
    sentMoment = datetime.datetime.now() - datetime.timedelta(days=random.randint(0, 400), minutes=random.randint(0, 59), seconds=random.randint(0, 59))
    receivedMoment = sentMoment + datetime.timedelta(seconds=random.randint(0, 59))
    f.write('<urn:uuid:' + emailUID + '> a nmo:Email; \n')
    f.write('\tnmo:to <urn:uuid:1>;\n')
    f.write('\tnmo:from <urn:uuid:' + UID + '>;\n')
    f.write('\tnmo:sentDate "' + sentMoment.isoformat().split('.')[0] + '";\n')
    f.write('\tnmo:receivedDate "' + receivedMoment.isoformat().split('.')[0] + '";\n')
    f.write('\tnmo:contentMimeType "text/plain";\n')
    #if random.randint(0, 11)>9 and len(previousContacts)>1: f.write('\tnmo:isRead true;\n')
    #else: f.write('\tnmo:isRead false;\n')
    f.write('\tnmo:messageHeader [a nmo:MessageHeader; nmo:headerName "User-Agent"; nmo:headerValue "tin/unoff-1.3-BETA-970813 (UNIX) (Linux/2.0.30 (i486)) "];\n')
    f.write('\tnmo:messageHeader [a nmo:MessageHeader; nmo:headerName "from"; nmo:headerValue "' + emailAddress + '"];\n')
    f.write('\tnmo:messageHeader [a nmo:MessageHeader; nmo:headerName "to"; nmo:headerValue "me@me.com"];\n')
    if random.randint(0, 10) > 6 and len(previousContacts) > 1:
        ccid = random.randint(0, len(previousContacts) - 1)
        f.write('\tnmo:cc <urn:uuid:' + previousContacts[ccid] + '>;\n')
        f.write('\tnmo:messageHeader [a nmo:MessageHeader; nmo:headerName "cc"; nmo:headerValue "' + previousEmailAddresses[ccid] + '"];\n')
    
        
    if random.randint(0, 10) > 7 and len(previousContacts) > 1:
        bccid = random.randint(0, len(previousContacts) - 1)
        f.write('\tnmo:bcc <urn:uuid:' + previousContacts[bccid] + '>;\n')
        f.write('\tnmo:messageHeader [a nmo:MessageHeader; nmo:headerName "bcc"; nmo:headerValue "' + previousEmailAddresses[bccid] + '"];\n')
    #TODO add some sense to the email titles. Some reply chains as well.
    if (random.randint(0, 4) > 3):
        f.write ('\tnao:hasTag [a nao:Tag ; nao:prefLabel ' +  getRandomTag () +'];\n')
    
    f.write('\tnmo:messageSubject "' + str.replace(gen_data.create_paragraphs(1, 2, 2), "\n", "") + '";\n')
    f.write('\tnmo:plainTextMessageContent "' + str.replace(gen_data.create_paragraphs(1, 2, 3), "\n", "") + '".\n')



# BEGIN CREATING.
if (len(sys.argv) < 2):
    print "Usage: python get_ttl.py NO_CONTACTS [--with-phone]"
    sys.exit(0)

try:
    count = int(sys.argv[1])
except:
    print "Usage: python get_ttl.py NO_CONTACTS [--with-phone]"
    sys.exit(0)

if (len(sys.argv) > 2):
    if (sys.argv[2] == "--with-phone") :
        print "Writing %d contacts in 'contacts.ttl' with phone information " %(count)
        withPhone = True
    else:
        print "WTF?!?!? python get_ttl.py NO_CONTACTS [--with-phone]"
else:
    print "Writing %d 'contacts.ttl' without phone information" % (count)
    print "Add --with-phone option to include phone information"
    withPhone = False
        
f=open("./contacts.ttl", 'w' )
f.write("@prefix rdfs:  <http://www.w3.org/2000/01/rdf-schema#>.\n")
f.write("@prefix nrl:   <http://www.semanticdesktop.org/ontologies/2007/08/15/nrl#>.\n")
f.write("@prefix nid3:   <http://www.semanticdesktop.org/ontologies/2007/05/10/nid3#>.\n")
f.write("@prefix nao:   <http://www.semanticdesktop.org/ontologies/2007/08/15/nao#>.\n")
f.write("@prefix nco:   <http://www.semanticdesktop.org/ontologies/2007/03/22/nco#>.\n")
f.write("@prefix nmo:   <http://www.semanticdesktop.org/ontologies/2007/03/22/nmo#>.\n")
f.write("@prefix nfo:   <http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#>.\n")    
f.write("@prefix nie:   <http://www.semanticdesktop.org/ontologies/2007/01/19/nie#>.\n")
f.write("@prefix ncal:   <http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#>.\n")

f.write("@prefix xsd:   <http://www.w3.org/2001/XMLSchema#>.\n")
f.write('<mailto:me@me.com> a nco:EmailAddress; \n')
f.write('\tnco:emailAddress "me@me.com".\n')
f.write('\n')
f.write('<urn:uuid:1> a nco:PersonContact; \n')    
f.write('\tnco:fullname "Me Myself";\n')
f.write('\tnco:nameGiven "Me";\n')
f.write('\tnco:nameFamily "Myself";\n')
f.write('\tnco:birthDate "2008-01-01";\n')
f.write('\tnco:hasEmailAddress <mailto:me@me.com>;\n')
f.write('\tnco:hasPhoneNumber <tel:+11111111111>.\n')
f.write('\n')
f.write('<tel:+11111111111> a nco:PhoneNumber; \n')
f.write('\tnco:phoneNumber "(111) 111-1111".\n')
f.write('\n')

#TODO need to create some email folders
myOwnPhoneNumberURI="tel:+11111111111"
previousContacts=[]
previousEmailAddresses=[]
previousIMAccounts=[]
allchars = string.maketrans('','')

for dummy in range (0, count):
    
    firstName, lastName = gen_data.create_name()
    zip, city, state = gen_data.create_city_state_zip()
    postalAddressID=str(random.randint(0, sys.maxint))
    
    UID=str(random.randint(0, sys.maxint))
    phoneNumber=gen_data.create_phone()
    phoneUri='tel:+1' + phoneNumber.translate(allchars,' -()')
    birthDay=gen_data.create_birthday()
    streetAddress=gen_data.create_street()
    emailAddress=gen_data.create_email(name=(firstName, lastName))
    xmppAddress=str(firstName+"."+lastName+"@gmail.com").lower()
    hasIMAccount=False
    hasPhoneNumber=False
    jobTitle=gen_data.create_job_title()

    generatePostalAddress()    
    generateEmailAddress()    
        
    #Only every 3rd have Phone or IM to add variation.
    if random.randint(0, 3)>2 or count==1: 
        generateIMAccount(gen_data, str)
        hasIMAccount=True
    if random.randint(0, 3)>2 or count==1:
        generatePhoneNumber()
        hasPhoneNumber=True
        if (withPhone): generatePhoneCalls(3)
        if (withPhone): generateSMS (4)
        
    f.write('<urn:uuid:'+UID+'> a nco:PersonContact; \n')    
    f.write('\tnco:fullname "'+firstName+ ' ' + lastName+'";\n')
    f.write('\tnco:nameGiven "'+firstName+'";\n')
    f.write('\tnco:nameFamily "'+lastName+'";\n')
    f.write('\tnco:birthDate "'+str(birthDay)+'";\n')
    #f.write('\tnco:title "'+jobTitle+'";\n')
    f.write('\tnco:hasEmailAddress <mailto:'+emailAddress+'>;\n')
    if hasPhoneNumber: f.write('\tnco:hasPhoneNumber <'+phoneUri+'>;\n')
    if hasIMAccount: f.write('\tnco:hasIMAccount <xmpp:'+xmppAddress+'>;\n')    	 
    if (random.randint(0, 4) > 3):
        f.write ('\tnao:hasTag [a nao:Tag ; nao:prefLabel ' +  getRandomTag () +'];\n')
    f.write('\tnco:hasPostalAddress <urn:uuid:'+postalAddressID+'>.\n')
    f.write('\n')

        
    #calendarEntryID=str(random.randint(0, sys.maxint))
    #if random.randint(0, 3)>2 and count>2 and len(previousContacts):
    #    generateCalendarEntry(gen_data, str, random)
    
    #20% Send emails. Those who do, send 1-30 emails. EMails have CC and BCC people     
    if random.randint(0, 10)>8 or count==1:
        emailcount=random.randint(1, 30)        
        for dummy in range (0, emailcount):
            generateEmail(sys, gen_data, str, random)
            f.write('\n')
    previousContacts.append(UID)
    previousEmailAddresses.append(emailAddress)
    
    #TODO INSERT IM - Use just a nmo:Message for that for now. 
    
    #TODO: Insert bookmarks
    
    
f.close()

