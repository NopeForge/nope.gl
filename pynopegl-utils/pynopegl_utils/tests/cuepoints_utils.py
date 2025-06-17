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

from typing import Mapping, Tuple

from pynopegl_utils.toolbox.colors import COLORS

import pynopegl as ngl


def get_grid_points(cols: int, rows: int) -> Mapping[str, Tuple[float, float]]:
    points = {}
    offx = 1 / (2 * cols)
    offy = 1 / (2 * rows)
    for y in range(rows):
        for x in range(cols):
            px = (x / cols + offx) * 2.0 - 1.0
            py = (y / rows + offy) * 2.0 - 1.0
            key = f"{x}{y}"
            if cols == 1:
                key = f"{y}"
            elif rows == 1:
                key = f"{x}"
            points[key] = (px, py)
    return points


def get_points_nodes(
    cfg: ngl.SceneCfg,
    points: Mapping[str, Tuple[float, float]],
    radius: float = 0.025,
    color: Tuple[float, float, float] = COLORS.green,
    text_size: Tuple[float, float] = (0.1, 0.1),
):
    g = ngl.Group(label="Debug circles")
    circle = ngl.Circle(radius=radius)
    circle_draw = ngl.DrawColor(color, geometry=circle)
    box = (-1.0, -1.0, text_size[0], text_size[1])
    for pos_name, position in points.items():
        text = ngl.Text(pos_name, box=box, bg_opacity=0, valign="top")
        text = ngl.Translate(text, (1 + radius, 1 - radius - text_size[1], 0))
        point = ngl.Group(children=[circle_draw, text])
        point = ngl.Translate(point, position + (0,))
        g.add_children(point)
    return g
