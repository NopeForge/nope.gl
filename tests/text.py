#
# Copyright 2020-2022 GoPro Inc.
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

from pynopegl_utils.misc import scene
from pynopegl_utils.tests.cmp_fingerprint import test_fingerprint
from pynopegl_utils.toolbox.colors import COLORS

import pynopegl as ngl


@test_fingerprint(tolerance=1)
@scene()
def text_0_to_127(_):
    s = ""
    for y in range(8):
        for x in range(16):
            c = y << 4 | x
            s += chr(c) if c else " "
        s += "\n"
    return ngl.Text(s)


def _text(**params):
    return ngl.Text("This\nis\nnope.gl", font_scale=0.7, padding=8, **params)


@test_fingerprint(tolerance=1)
@scene()
def text_colors(_):
    return _text(fg_color=COLORS.rose, bg_color=COLORS.cgreen, bg_opacity=1)


@test_fingerprint(tolerance=1)
@scene()
def text_align_cc(_):
    return _text(valign="center", halign="center")


@test_fingerprint(tolerance=1)
@scene()
def text_align_cr(_):
    return _text(valign="center", halign="right")


@test_fingerprint(tolerance=1)
@scene()
def text_align_cl(_):
    return _text(valign="center", halign="left")


@test_fingerprint(tolerance=1)
@scene()
def text_align_bc(_):
    return _text(valign="bottom", halign="center")


@test_fingerprint(tolerance=1)
@scene()
def text_align_br(_):
    return _text(valign="bottom", halign="right")


@test_fingerprint(tolerance=1)
@scene()
def text_align_bl(_):
    return _text(valign="bottom", halign="left")


@test_fingerprint(tolerance=1)
@scene()
def text_align_tc(_):
    return _text(valign="top", halign="center")


@test_fingerprint(tolerance=1)
@scene()
def text_align_tr(_):
    return _text(valign="top", halign="right")


@test_fingerprint(tolerance=1)
@scene()
def text_align_tl(_):
    return _text(valign="top", halign="left")
