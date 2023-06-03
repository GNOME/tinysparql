#!/usr/bin/env python3
import json, sys

index = json.load(open(sys.argv[1]))
for x in range (2, len(sys.argv)):
    extra = json.load(open(sys.argv[x]))
    index['symbols'] += extra['symbols']

rewritten = json.dumps(index)

with open(sys.argv[1], 'w') as f:
    f.write(rewritten)
    f.close()
