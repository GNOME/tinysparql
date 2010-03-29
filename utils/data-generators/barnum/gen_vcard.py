#! /usr/bin/python
#
# Copyright (C) 2009, Nokia
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

import sys, random
import gen_data

try:
    count = int (sys.argv[1])
except:
    count = 1

vcard = """BEGIN:VCARD
VERSION:3.0
UID:barnum-%d
FN:%s
N:%s;%s;;;
BDAY:%s
EMAIL;TYPE=WORK:%s
X-JABBER;TYPE=HOME:%s
ADR;TYPE=HOME:;;%s;%s;%s;%s;USA
END:VCARD"""

for dummy in range (0, count):
    name = gen_data.create_name()
    adr = gen_data.create_city_state_zip()

    print vcard % (random.randint(0, sys.maxint),
                   ' '.join(name),
                   name[1], name[0],
                   gen_data.create_birthday(),
                   gen_data.create_email(name=name),
                   gen_data.create_email(name=name),
                   gen_data.create_street(),
                   adr[1], adr[2], adr[0],
                   )
