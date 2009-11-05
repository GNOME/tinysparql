#!/usr/bin/env python2.5
import random, sys
from internals.tools import print_namespaces, print_property, getPseudoRandomDate

def gen_bookmark_uuid ():
    return "urn:uuid:" + str(random.randint(0, sys.maxint))



def gen_bookmarks_ttl (filename):

    print_namespaces ()

    for line in open (filename, 'r'):
        if (line.startswith ("#")):
            continue

        url, title, tags = line.split('|')
        print "<%s> a nfo:Bookmark;" % (gen_bookmark_uuid ())
        print_property ("nie:title", title)
        print_property ("nie:contentCreated", getPseudoRandomDate ())
        for tag in tags.strip().split(','):
            print '\tnao:hasTag [a nao:Tag; nao:prefLabel "%s"];' % (tag.strip())
        
        print_property ("nfo:bookmarks", url, "uri", True)
        

if __name__ == "__main__":
    gen_bookmarks_ttl ("./bookmarks.in")

