import ConfigParser, os
import sys

config = ConfigParser.ConfigParser()
config.readfp(sys.stdin)

for section in config.sections():
   splitted = section.split (":")
   print "<section prefix=\"" + splitted[0] + "\" name=\"" + splitted[1] + "\" fullname=\"" + section + "\">"
   for name, value in config.items (section):
     vsplit = value.split (":")
     print "\t<item name=\""+ name + "\" fullvalue=\"" + value+ "\">"
     if len (vsplit) > 1:
       print "\t\t<prefix>" + vsplit[0] + "</prefix>"
       print "\t\t<value>" + vsplit[1] + "</value>"
     else:
       print "\t\t<prefix />"
       print "\t\t<value>" + value + "</value>"
     print "\t</item>"
   print "</section>"
