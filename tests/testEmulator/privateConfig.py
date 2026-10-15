#!/usr/bin/env python3
"""Writes a copy of a test configuration whose output files (every string starting with /tmp/) are
moved into a private directory, so concurrent test runs never share output files.

usage: privateConfig.py <config> <directory> [key=value ...]
Prints the path of the copy (<directory>/<config basename>). Each key=value sets a top-level path
'Section/Subsection/Key' to a JSON value, e.g. 'Driver Configuration/Win State Collection/Max Files=1'.
"""
import json
import os
import sys

source, directory = sys.argv[1], sys.argv[2]
config = json.load(open(source))


def relocate(node):
    if isinstance(node, dict):
        return {k: relocate(v) for k, v in node.items()}
    if isinstance(node, list):
        return [relocate(v) for v in node]
    if isinstance(node, str) and node.startswith('/tmp/'):
        return os.path.join(directory, node[len('/tmp/'):])
    return node


config = relocate(config)
for assignment in sys.argv[3:]:
    path, value = assignment.split('=', 1)
    keys = path.split('/')
    target = config
    for key in keys[:-1]:
        target = target[key]
    target[keys[-1]] = json.loads(value)

out = os.path.join(directory, os.path.basename(source))
json.dump(config, open(out, 'w'), indent=1)
print(out)
