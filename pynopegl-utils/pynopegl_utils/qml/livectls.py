#
# Copyright 2023 Nope Foundry
# Copyright 2022 GoPro Inc.
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

from typing import Any, Dict, List

from PySide6.QtGui import QColor

import pynopegl as ngl

_NODE_TYPES_MODEL_MAP = dict(
    UniformColor="color",
    UniformBool="bool",
    UniformVec2="vector",
    UniformVec3="vector",
    UniformVec4="vector",
    UniformMat4="vector",
    Text="text",
)

_NODE_TYPES_CMP_MAP = dict(
    UniformVec2=2,
    UniformVec3=3,
    UniformVec4=4,
    UniformMat4=4 * 4,
)


def get_model_data(scene) -> List[Dict[str, Any]]:
    livectls = ngl.get_livectls(scene)
    model_data = []
    for label, ctl in livectls.items():
        data = dict(
            type=_NODE_TYPES_MODEL_MAP.get(ctl["node_type"], "range"),
            label=label,
            val=ctl["val"],
            node=ctl["node"],
        )
        if data["type"] not in ("bool", "text"):
            data["min"] = ctl["min"]
            data["max"] = ctl["max"]
        if data["type"] == "vector":
            data["n"] = _NODE_TYPES_CMP_MAP.get(ctl["node_type"], 0)
        if data["type"] == "color":
            data["val"] = QColor.fromRgbF(*ctl["val"])
        model_data.append(data)
    return model_data


def apply_changes(changes: List[Dict[str, Any]]):
    for ctl in changes:
        node = ctl["node"]
        type_ = ctl["type"]
        value = ctl["val"]
        if type_ in ("range", "bool"):
            node.set_value(value)
        elif type_ == "color":
            node.set_value(*QColor.getRgbF(value)[:3])
        elif type_ == "text":
            node.set_text(value)
        elif type_ == "vector":
            value = [value.property(i).toNumber() for i in range(value.property("length").toInt())]
            node.set_value(*value)
