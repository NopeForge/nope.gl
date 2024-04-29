#!/usr/bin/env python3

"""
Extract the changelog for a given tag or version
"""

import itertools
import sys

if len(sys.argv) != 2:
    print(f"Usage: {sys.argv[0]} <tag_or_version>", file=sys.stderr)
    sys.exit(1)

version = sys.argv[1]
if version.startswith("v"):
    version = version[1:]

lines = open("CHANGELOG.md").read().splitlines()
iter_lines = iter(lines)
iter_lines = itertools.dropwhile(lambda line: not line.startswith(f"## [{version}] "), iter_lines)

output_lines = ["# Changelog", ""]
try:
    output_lines.append(next(iter_lines))
except StopIteration:
    print(f"Version {version} not found in the changelog", file=sys.stderr)

output_lines += itertools.takewhile(lambda line: not line.startswith(f"## ["), iter_lines)

print("\n".join(output_lines).rstrip())
