#!/bin/sh
#
# Run all ttl generators, assumes a generator is prefixed by gen_
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

if [ -z $1 ]
then
entries=2000
else
entries=$1
fi

echo "Generating $entries entries per category"

for generator in `ls generate-data-for-*.py`
do
   echo "Running $generator"
   dest=`echo $generator | sed -s "s/\.py/\.ttl/"`

   if test "x$generator" = "xgenerate-data-for-music.py"; then
       args="-T $entries"
   else
       args="$entries"
   fi

   ./$generator $args > $dest
done
