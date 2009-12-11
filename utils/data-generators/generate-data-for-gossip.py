#!/usr/bin/env python
import random, sys
import barnum.gen_data as gen_data
from internals.tools import print_property, get_random_uuid_uri, get_random_in_list
from internals.tools import print_namespaces, print_instance, print_anon_node
from internals.tools import getPseudoRandomDate

# (Protocol, Communication channels, uri-prefix, me-address)
ACCOUNTS = [("Skype", ["voice", "video", "conversation"],
             "skype:", "skype:me@skypemail.com"),
            
            ("MSN", ["conversation"],
             "msn:", "msn:me@hotmail.com"),
            
            ("GTalk", ["voice", "video", "conversation"],
             "gtalk:", "gtalk:me@gmail.com")
            ]

MAIL_ADDRESSES = ["me@gmail.com", "me@nokia.com", "me@hotmail.com", "me@gmx.de"]
REPLY_TO = "spam@triluriluri.com"

#
# Status: 0 - Offline, 1 - Online, 2 - Away, 3 - Peeing, 4 - ...
#
NICKNAME_EXTENSIONS = ["the brave", "the coward", "the good", "the bad", "the ugly"]
ACCOUNT_EXTENSIONS = ["at home", "at office", "mobile"]

def get_nickname (firstname):
    return str.join (' ', [firstname, get_random_in_list (NICKNAME_EXTENSIONS)])

def get_account_name (protocol):
    return str.join (' ', [protocol, get_random_in_list (ACCOUNT_EXTENSIONS)])

def get_random_text ():
    return str.replace(gen_data.create_paragraphs(1, 2, 3), "\n", "").strip()

def get_random_text_short ():
    return str.replace(gen_data.create_paragraphs(1, 1, 2), "\n", "").strip()
    
def get_random_message_id (length=12):
    CHARS="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890"
    return ''.join([CHARS[random.randint (0, len(CHARS)-1)] for i in range(length)])

def gen_account (account_data, user_account, firstname):
    print_instance (user_account, "nco:IMAccount")
    #print_property ("nco:imStatus", random.randint (0, 5), t="int")
    print_property ("nco:imAccountType", get_account_name (account_data[0]))
    print_property ("nco:imProtocol", account_data[2][:-1])
    print_property ("nco:imNickname", get_nickname (firstname), final=True)

    
def gen_users_and_accounts (amount):

    known_accounts = []
    known_emails = []
    
    for person in range (0, amount):
        accounts = []
        firstName, lastName = gen_data.create_name()
        zip, city, state = gen_data.create_city_state_zip()
        postalAddressID = str(random.randint(0, sys.maxint))
    
        UID = str (random.randint (0, sys.maxint))
        birthDay = gen_data.create_birthday ()
        streetAddress = gen_data.create_street ()
        emailAddress = gen_data.create_email (name = (firstName, lastName))
        print_instance (get_random_uuid_uri(), "nco:PersonContact")
      
        print_property ("nco:fullname", str.join(' ', [firstName, lastName]))
        print_property ("nco:nameGiven", firstName)
        print_property ("nco:nameFamily", lastName)
        print_property ("nco:birthDate", str(birthDay))

        for j in range (0, random.randint(0, 4)):
            account_data = get_random_in_list (ACCOUNTS)
            user_account = str.join ('', [account_data[2], str(j), emailAddress])
            print_property ("nco:hasIMAccount", user_account, t="uri")
            accounts.append ((user_account, account_data))
            known_accounts.insert (0, user_account)
            known_emails.insert (0, emailAddress)

        print_property ("nco:hasEmailAddress",
                        str.join ('', ["mailto:", emailAddress]),
                        t="uri", final=True)

        for user_account, account_data in accounts:
            gen_account (account_data, user_account, firstName)
            del user_account
            del account_data

        print_instance ("mailto:" + emailAddress, "nco:EmailAddress")
        print_property ("nco:emailAddress", emailAddress, final=True)
            
    return known_accounts, known_emails

def get_folder_names (address):
    folders =  ["Inbox", "Outbox", "Sent"]
    return map (lambda x : "mailfolder://" + address + "/" + x, folders)

def gen_mailfolder (folderuri):
    # mailfolder://a@b.com/x/y/x ----> foldername X, displayName x
    print_instance (folderuri, "nmo:MailFolder")
    print_property ("nmo:folderName", folderuri.rpartition ('/')[2].upper (), final=True)
    #print_property ("nmo:folderDisplayName",  folderuri.rpartition ('/')[2])
    #print_property ("nmo:status", "abudabu", final=True)

def gen_me ():
    print_instance ("urn:uuid:buddy1", "nco:PersonContact")
    print_property ("nco:fullname", "Me myself")
    print_property ("nco:nameGiven", "Me")
    print_property ("nco:nameFamily", "Myself")
    print_property ("nco:birthDate", "1980-01-01")

    # Email accounts"
    for address in MAIL_ADDRESSES:
        print_property ("nco:hasEmailAddress", "mailto:" + address, t="uri")

    print_property ("nco:hasIMAccount", "msn:me@hotmail.com", t="uri")
    print_property ("nco:hasIMAccount", "gtalk:me@gmail.com", t="uri")
    print_property ("nco:hasIMAccount", "skype:me@hotmail.com", t="uri", final=True)

    for account in ACCOUNTS:
        gen_account (account, account[3], "Me")

    # Email addresses and accounts
    # (FIXME: it should be n:m instead of 1:1)
    i = 0
    for address in MAIL_ADDRESSES:
        print_instance ("mailto:" + address, "nco:EmailAddress")
        print_property ("nco:emailAddress", address, final=True)

        print_instance (get_random_uuid_uri (), "nmo:MailAccount")
        print_property ("nmo:accountName",
                        "Mail " + str(i))
        print_property ("nmo:accountDisplayName",
                        "Mail " + get_random_in_list (ACCOUNT_EXTENSIONS))
        print_property ("nmo:fromAddress",
                        "mailto:" + address, t="uri")
        for folder in get_folder_names (address):
            print_property ("nie:hasLogicalPart",
                            folder, t="uri")
        print_property ("nmo:signature", get_random_text(), final=True)

        for folder in get_folder_names (address):
            gen_mailfolder (folder)
        i += 1
        #print_property ("nmo:folderStatus", "?")
        #print_property ("nmo:folderParameters", "?")


def gen_gossip (amount, friends_list):

    for i in range (0, amount):
        random_friend = get_random_in_list (friends_list)
        my_account_prefix = random_friend.split(':')[0] + ":"
        my_account = filter (lambda x: x[3].startswith (my_account_prefix), ACCOUNTS)[0]
        addresses = [my_account[3], random_friend]

        for i in range (0, random.randint (0, 10)):
            if ("voice" in my_account[1]):
                print_instance (get_random_uuid_uri(), "nmo:VOIPCall")
                print_property ("nmo:duration", random.randint (10,300),
                                t="int")
            else:
                print_instance (get_random_uuid_uri(), "nmo:IMMessage")

            if (i%2 == 0):
                print_property ("nmo:sentDate", getPseudoRandomDate ())
            else:
                print_property ("nmo:receivedDate", getPseudoRandomDate ())
            print_anon_node ("nmo:from", "nco:Contact",
                             "nco:hasIMAccount", addresses [i%2], t="uri")
            print_anon_node ("nmo:to", "nco:Contact",
                             "nco:hasIMAccount", addresses [(i+1)%2], t="uri")
            print_property ("nmo:htmlMessageContent", get_random_text (), final=True)


def gen_mail (amount, known_emails):

    for i in range (0, amount):
        random_address_friend = "mailto:" + get_random_in_list (known_emails)
        random_address_me = "mailto:" + get_random_in_list (MAIL_ADDRESSES)

        print_instance (get_random_uuid_uri (), "nmo:Email")
        # Half sent, half received
        if (i%2 == 0):
            #sent
            print_anon_node ("nmo:from", "nco:Contact",
                             "nco:hasEmailAddress", random_address_me, t="uri")
            print_anon_node ("nmo:to", "nco:Contact",
                             "nco:hasEmailAddress", random_address_friend, t="uri")
            print_property ("nmo:sentDate", getPseudoRandomDate ())
            print_property ("nie:isLogicalPartOf",
                            "mailfolder://" + random_address_me[7:] + "/Sent", t="uri")
        else:
            #received
            print_anon_node ("nmo:to", "nco:Contact",
                             "nco:hasEmailAddress", random_address_me, t="uri")
            print_anon_node ("nmo:from", "nco:Contact",
                             "nco:hasEmailAddress", random_address_friend, t="uri")
            print_property ("nmo:receivedDate", getPseudoRandomDate ())
            print_property ("nie:isLogicalPartOf",
                            "mailfolder://" + random_address_me[7:] + "/Inbox", t="uri")

        if (random.randint (0, 5) > 3):
            print_anon_node ("nmo:cc", "nco:Contact",
                             "nco:hasEmailAddress",
                             "mailto:" + get_random_in_list (known_emails), t="uri")

        if (random.randint (0, 5) > 3):
            print_anon_node ("nmo:bcc", "nco:Contact",
                             "nco:hasEmailAddress",
                             "mailto:" + get_random_in_list (known_emails), t="uri")

        print_property ("nmo:messageSubject", get_random_text_short ())
        print_property ("nmo:status", "eeeeeh uuuhhhmmmmm")
        print_property ("nmo:responseType", "blublublu")
        print_property ("nmo:messageId", "<" + get_random_message_id () + "@email.net>")
        print_property ("nmo:plainTextMessageContent", get_random_text ())
        print_anon_node ("nmo:replyTo", "nco:Contact",
                         "nco:hasEmailAddress",
                         REPLY_TO, t="uri")
        # FIXME Add message headers
        # FIXME Add inReplyTo
        print_property ("nie:contentSize", random.randint (20, 120), t="str", final=True)
        
    
if __name__ == "__main__":

    #sys.argv = ["blabla", 5]

    if (len(sys.argv) < 2):
        print "Usage: %s NO_ENTRIES (it must be an integer > 0" % (__name__)
        sys.exit(-1)

    try:
        entries = int (sys.argv[1])
    except ValueError:
        print "Usage: %s NO_ENTRIES (it must be an integer > 0)" % (__name__)
        sys.exit (-1)
        
    if (entries < 0):
        print >> sys.stderr, "Entries must be > 0"
        sys.exit(-1)
    
    print_namespaces ()
    gen_me ()
    known_accounts, known_emails = gen_users_and_accounts (entries)
#    gen_gossip (80, known_accounts)
    gen_mail (80, known_emails)
