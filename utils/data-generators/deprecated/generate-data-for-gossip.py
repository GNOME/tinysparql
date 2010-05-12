#!/usr/bin/env python
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

import random, sys, os
import barnum.gen_data as gen_data
from internals.tools import print_property, get_random_uuid_uri, get_random_in_list
from internals.tools import print_namespaces, print_instance, print_anon_node
from internals.tools import getPseudoRandomDate

# Accounts: protocol, communication channels, uri-prefix, me-address
ACCOUNTS = [("Skype", 
             ["voice", 
              "video", 
              "conversation"],
             "skype:", 
             "skype:me@skypemail.com"),
            ("MSN", 
             ["conversation"],
             "msn:", 
             "msn:me@hotmail.com"),
            ("GTalk", 
             ["voice", 
              "video", 
              "conversation"],
             "gtalk:", 
             "gtalk:me@gmail.com")
            ]

MAIL_ADDRESSES = ["me@gmail.com", 
                  "me@nokia.com", 
                  "me@hotmail.com", 
                  "me@gmx.de"]

REPLY_TO = "spam@sliff.com"

#
# Status: 0 - Offline, 1 - Online, 2 - Away, 3 - Peeing, 4 - ...
#
NICKNAME_EXTENSIONS = ["the brave", 
                       "the coward", 
                       "the good", 
                       "the bad", 
                       "the ugly"]
ACCOUNT_EXTENSIONS = ["Home", 
                      "Office", 
                      "Mobile"]

def get_nickname (firstname):
    #return str.join (' ', [firstname, get_random_in_list (NICKNAME_EXTENSIONS)])
    return generate_name()

def get_account_name (protocol):
    return str.join (' ', [get_random_in_list (ACCOUNT_EXTENSIONS), protocol])

def get_random_text ():
    return str.replace(gen_data.create_paragraphs(1, 2, 3), "\n", "").strip()

def get_random_text_short ():
    return str.replace(gen_data.create_paragraphs(1, 1, 2), "\n", "").strip()
    
def get_random_message_id (length=12):
    CHARS="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890"
    return ''.join([CHARS[random.randint (0, len(CHARS)-1)] for i in range(length)])

def generate_fullname():
        name = os.popen('./generate-name.py').read()

        first_name = ""
        last_name = ""

 	for line in name.splitlines():
                if not first_name:
                        first_name = line
                        continue

                if not last_name:
                        last_name = line
                        continue

        full_name = '%s %s' % (first_name, last_name)

        return full_name

def generate_name():
        name = os.popen('./generate-name.py').read()

        first_name = ""
        last_name = ""

 	for line in name.splitlines():
            return line

        return "foo"

def generate_account (account_data, user_account, firstname):
    print_instance (user_account, "nco:IMAccount")
    #print_property ("nco:imStatus", random.randint (0, 5), t = "int")
    print_property ("nco:imAccountType", get_account_name (account_data[0]))
    print_property ("nco:imProtocol", account_data[2][:-1])
    print_property ("nco:imNickname", get_nickname (firstname), final = True)

def generate_accounts (amount):
    known_accounts = []
    
    for person in range (0, amount):
        accounts = []
        first_name, last_name = gen_data.create_name()
        zip, city, state = gen_data.create_city_state_zip()
        address_id = str(random.randint(0, sys.maxint))
    
        UID = str (random.randint (0, sys.maxint))
        birth_day = gen_data.create_birthday ()
        street_address = gen_data.create_street ()
        email_address = gen_data.create_email (name = (first_name, last_name))

        print_instance (get_random_uuid_uri(), "nco:PersonContact")
        print_property ("nco:fullname", str.join(' ', [first_name, last_name]))
        print_property ("nco:nameGiven", first_name)
        print_property ("nco:nameFamily", last_name)

        for j in range (0, random.randint(0, 4)):
            account_data = get_random_in_list (ACCOUNTS)
            user_account = str.join ('', [account_data[2], str(j), email_address])
            print_property ("nco:hasIMAccount", user_account, t = "uri")
            accounts.append ((user_account, account_data))
            known_accounts.insert (0, user_account)

        print_property ("nco:birthDate", str(birth_day), final = True)
            
    return known_accounts

def get_folder_names (address):
    folders = ["Inbox", "Outbox", "Sent"]
    return map (lambda x : "mailfolder://" + address + "/" + x, folders)

def generate_mailfolder (folderuri):
    # mailfolder://a@b.com/x/y/x ----> foldername X, displayName x
    print_instance (folderuri, "nmo:MailFolder")
    print_property ("nmo:folderName", folderuri.rpartition ('/')[2].upper (), final = True)
    #print_property ("nmo:folderDisplayName",  folderuri.rpartition ('/')[2])
    #print_property ("nmo:status", "abudabu", final=True)

def generate_me ():
    print "nco:default-contact-me a nco:PersonContact ;"
    print_property ("nco:fullname", "Me Nokia")
    print_property ("nco:nameGiven", "Me")
    print_property ("nco:nameFamily", "Nokia")
    print_property ("nco:birthDate", "1980-01-01")

    # Email accounts
    for address in MAIL_ADDRESSES:
        print_property ("nco:hasEmailAddress", "mailto:" + address, t = "uri")

    print_property ("nco:hasIMAccount", "msn:me@hotmail.com", t = "uri")
    print_property ("nco:hasIMAccount", "gtalk:me@gmail.com", t = "uri")
    print_property ("nco:hasIMAccount", "skype:me@hotmail.com", t = "uri", final = True)

    for account in ACCOUNTS:
        generate_account (account, account[3], "Me")

    # Email addresses and accounts
    # (FIXME: it should be n:m instead of 1:1)
    i = 0
    for address in MAIL_ADDRESSES:
        print_instance ("mailto:" + address, "nco:EmailAddress")
        print_property ("nco:emailAddress", address, final = True)

        print_instance (get_random_uuid_uri (), "nmo:MailAccount")
        print_property ("nmo:accountName",
                        "Mail " + str(i))
        print_property ("nmo:accountDisplayName",
                        get_random_in_list (ACCOUNT_EXTENSIONS) + " EMail")
        print_property ("nmo:fromAddress",
                        "mailto:" + address, t = "uri")
        for folder in get_folder_names (address):
            print_property ("nie:hasLogicalPart",
                            folder, t = "uri")
        print_property ("nmo:signature", get_random_text(), final = True)

        #for folder in get_folder_names (address):
        #    generate_mailfolder (folder)

        i += 1
        #print_property ("nmo:folderStatus", "?")
        #print_property ("nmo:folderParameters", "?")

def generate_im_messages (n_convs, n_channels, msgs_per_convs, friends_list, n_friends):
    for i in range (0, len(friends_list)):
        same_friend = friends_list[i]

        channel_id = get_random_uuid_uri()
        print_instance (channel_id, "nmo:CommunicationChannel")

        print_property ("nmo:lastMessageDate", getPseudoRandomDate ())

        print "\tnmo:hasParticipant nco:default-contact-me ;";

        if same_friend:
            print_anon_node ("nmo:hasParticipant",
                             "nco:IMContact",
                             "nco:hasIMAccount",
                             same_friend,
                             t = "uri", final = True)

        for j in range (0, n_convs):
            conversation_id = get_random_uuid_uri()
            print_instance (conversation_id, "nmo:Conversation", final = True)

            if same_friend:
                random_friend = same_friend
            else:
                random_friend = get_random_in_list(friends_list)

            my_account_prefix = random_friend.split(':')[0] + ":"
            my_account = filter (lambda x: x[3].startswith (my_account_prefix), ACCOUNTS)[0]
            addresses = [my_account[3], random_friend]

            for k in range (0, random.randint(1, msgs_per_convs + 1)):
                print_instance (get_random_uuid_uri(), "nmo:IMMessage")

                print_property ("nmo:communicationChannel",
                                channel_id,
                                t = "uri")

                print_property ("nmo:conversation", 
                                conversation_id, 
                                t = "uri")
                if k % 2 == 0:
                    print_property ("nmo:sentDate", getPseudoRandomDate ())
                else:
                    print_property ("nmo:sentDate", getPseudoRandomDate ())
                    print_property ("nmo:receivedDate", getPseudoRandomDate ())

                print_property ("nie:plainTextContent", 
                                get_random_text ())

                print_anon_node ("nmo:from", 
                                 "nco:IMContact",
                                 "nco:hasIMAccount", 
                                 addresses[k % 2], 
                                 t = "uri")
                print_anon_node ("nmo:to", 
                                 "nco:IMContact",
                                 "nco:hasIMAccount", 
                                 addresses[(k + 1) % 2], 
                                 t = "uri", final = True)


def generate_im_group_chats (amount, friends_list):
    for i in range (0, amount):
        random_friend = get_random_in_list (friends_list)
        my_account_prefix = random_friend.split(':')[0] + ":"
        my_account = filter (lambda x: x[3].startswith (my_account_prefix), ACCOUNTS)[0]
        addresses = [my_account[3], random_friend]

        print_instance (get_random_uuid_uri(), "nmo:CommunicationChannel")

        for i in range (0, random.randint (0, 10)):
            print_instance (get_random_uuid_uri(), "nmo:IMMessage")

            if i % 2 == 0:
                print_property ("nmo:sentDate", getPseudoRandomDate ())
            else:
                print_property ("nmo:sentDate", getPseudoRandomDate ())
                print_property ("nmo:receivedDate", getPseudoRandomDate ())

            print_anon_node ("nmo:from", 
                             "nco:IMContact",
                             "nco:hasIMAccount", 
                             addresses[i % 2], 
                             t = "uri")
            print_anon_node ("nmo:to", 
                             "nco:IMContact",
                             "nco:hasIMAccount", 
                             addresses[(i + 1) % 2], 
                             t = "uri", final = True)

            print_property ("nie:plainTextContent", get_random_text (), t = "str", final = True)

def generate_mail (amount, known_emails):
    for i in range (0, amount):
        random_address_friend = "mailto:" + get_random_in_list (known_emails)
        random_address_me = "mailto:" + get_random_in_list (MAIL_ADDRESSES)

        print_instance (get_random_uuid_uri (), "nmo:Email")

        # Half sent, half received
        if i % 2 == 0:
            #sent
            print_anon_node ("nmo:from", "nco:Contact",
                             "nco:hasEmailAddress", random_address_me, t = "uri")
            print_anon_node ("nmo:to", "nco:Contact",
                             "nco:hasEmailAddress", random_address_friend, t = "uri")
            print_property ("nmo:sentDate", getPseudoRandomDate ())
            print_property ("nie:isLogicalPartOf",
                            "mailfolder://" + random_address_me[7:] + "/Sent", t = "uri")
        else:
            #received
            print_anon_node ("nmo:to", "nco:Contact",
                             "nco:hasEmailAddress", random_address_me, t = "uri")
            print_anon_node ("nmo:from", "nco:Contact",
                             "nco:hasEmailAddress", random_address_friend, t = "uri")
            print_property ("nmo:receivedDate", getPseudoRandomDate ())
            print_property ("nie:isLogicalPartOf",
                            "mailfolder://" + random_address_me[7:] + "/Inbox", t = "uri")

        if random.randint (0, 5) > 3:
            print_anon_node ("nmo:cc", "nco:Contact",
                             "nco:hasEmailAddress",
                             "mailto:" + get_random_in_list (known_emails), t = "uri")

        if random.randint (0, 5) > 3:
            print_anon_node ("nmo:bcc", "nco:Contact",
                             "nco:hasEmailAddress",
                             "mailto:" + get_random_in_list (known_emails), t = "uri")

        print_property ("nmo:messageSubject", get_random_text_short ())
        print_property ("nmo:status", "eeeeeh uuuhhhmmmmm")
        print_property ("nmo:responseType", "blublublu")
        print_property ("nmo:messageId", "<" + get_random_message_id () + "@email.net>")
        print_property ("nie:plainTextContent", get_random_text ())
        print_anon_node ("nmo:replyTo", "nco:Contact",
                         "nco:hasEmailAddress",
                         REPLY_TO, t = "uri")

        # FIXME Add message headers
        # FIXME Add inReplyTo
        print_property ("nie:contentSize", random.randint (20, 120), t = "str", final = True)
    
if __name__ == "__main__":
    #sys.argv = ["blabla", 5]

    if len(sys.argv) < 2:
        print "Expected number of entries to be provided (> 0)"
        print "Usage: %s NUMBER_OF_ENTRIES" % sys.argv[0]
        sys.exit(-1)

    try:
        entries = int (sys.argv[1])
    except ValueError:
        print "Expected number of entries to be provided (> 0)"
        print "Usage: %s NUMBER_OF_ENTRIES" % sys.argv[0]
        sys.exit (-1)
        
    if entries < 0:
        print >> sys.stderr, "Entries must be > 0"
        sys.exit(-1)
    
    print_namespaces ()
    generate_me ()
    known_accounts = generate_accounts (100)
    generate_im_messages (10, None, 100, known_accounts, 1)
#    generate_im_group_chats (entries, known_accounts)
#    generate_mail (entries, known_emails)
