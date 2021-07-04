#!/usr/bin/env python

"""
This is used to define Aberth-Ehrlich constants in distmap.frag
"""

import math
import sys

n = int(sys.argv[1])
for k in range(n):
    angle = 2 * math.pi / n
    off = math.pi / (2 * n)
    z = angle * k + off
    c, s = math.cos(z), math.sin(z)
    print(f"#define K{n}_{k} vec2({c:18.15f}, {s:18.15f})")
