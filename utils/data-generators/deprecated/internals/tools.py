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

import datetime, sys
import random

NAMESPACES = [
    	("rdf", "<http://www.w3.org/2000/01/rdf-schema#>"),
	("nrl", "<http://www.semanticdesktop.org/ontologies/2007/08/15/nrl#>"),
	("nid3","<http://www.semanticdesktop.org/ontologies/2007/05/10/nid3#>"),
	("nao", "<http://www.semanticdesktop.org/ontologies/2007/08/15/nao#>"),
	("nco", "<http://www.semanticdesktop.org/ontologies/2007/03/22/nco#>"),
	("nmo", "<http://www.semanticdesktop.org/ontologies/2007/03/22/nmo#>"),
	("nfo", "<http://www.semanticdesktop.org/ontologies/2007/03/22/nfo#>"),
	("nie", "<http://www.semanticdesktop.org/ontologies/2007/01/19/nie#>"),
	("ncal", "<http://www.semanticdesktop.org/ontologies/2007/04/02/ncal#>"),
	("xsd", "<http://www.w3.org/2001/XMLSchema#>")
    ]

def print_namespaces ():
    for prefix, uri in NAMESPACES:
        print "@prefix %s: %s." % (prefix, uri)
    print ""


def print_property (property_name, value, t="str", final=False):

    if (value):
        if (final):
            end_line = ".\n"
        else:
            end_line = ";"
        try:
            
            if (t == "str"):
                print '\t%s "%s"%s' % (property_name.encode ('utf8'),
                                       str(value).encode ('utf8').replace('"','\\"').replace('\n','\\n'), end_line)
            elif (t == "uri"):
                print '\t%s <%s>%s' % (property_name.encode ('utf8'),
                                       value.encode ('utf8'), end_line)
            elif (t == "int"):
                print '\t%s %s%s' % (property_name.encode ('utf8'),
                                     str(value), end_line)
        except (UnicodeDecodeError, UnicodeEncodeError):
            print >> sys.stderr, "Encoding error in %s %s %s" % (property_name,
                                                                 value,
                                                                 end_line)

def print_instance (uri, klass, final=False):
    if (final):
        append = "."
    else:
        append = ";"
    print "<%s> a %s%s" % (uri, klass, append)

def print_anon_node (prop, objtype, objprop, objpropvalue, t="str", final=False):

    delimiter_before = "\""
    delimiter_after  = "\""
    if (t == "uri"):
        delimiter_before = "<"
        delimiter_after = ">"

    if (final):
        end = ".\n"
    else:
        end = ";"
    
    print "\t%s [a %s; %s %s%s%s]%s" % (prop, objtype, objprop,
                                        delimiter_before, objpropvalue, delimiter_after,
                                        end)


def getPseudoRandomDate ():
    moment = datetime.datetime.now() - datetime.timedelta(days=random.randint(0, 400), minutes=random.randint(0, 59), seconds=random.randint(0, 59))
    return moment.isoformat ().split('.')[0]

def get_random_uuid_uri ():
    return "urn:uuid:" + str(random.randint(0, sys.maxint))

def get_random_in_list (l):
    if len(l) == 0:
        return None

    return l[random.randint (0, len(l) - 1)]

