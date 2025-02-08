#!/usr/bin/env python3

import os, sys, re

f = open(sys.argv[1])
content = f.read()
f.close()

dirname = os.path.dirname(sys.argv[1])

regex = re.compile('{{(.+?)}}')
matches = regex.findall(content)
replacements = {}

for m in matches:
    try:
        f = open(os.path.join(dirname, m.strip()))
        embedded = f.read()
    except:
        if (len(sys.argv) > 2):
            try:
                f = open (os.path.join(sys.argv[3], m.strip()))
                embedded = f.read()
            except:
                embedded = ''

    escaped = embedded.replace('\\', '\\\\')
    replace = re.compile('{{' + m + '}}')
    content = replace.sub(escaped, content)
    f.close()

f = open(sys.argv[2], 'w')
f.write(content)
f.close()
