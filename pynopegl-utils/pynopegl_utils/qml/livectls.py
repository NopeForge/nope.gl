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

from dataclasses import dataclass
from typing import Any, Dict, List

from PySide6.QtGui import QColor

import pynopegl as ngl


@dataclass
class _ControlSpec:
    control_type: str
    base_type: str
    components: int


_NODE_TYPES_SPEC_MAP = dict(
    Text=_ControlSpec("text", "str", 1),
    UniformBool=_ControlSpec("bool", "bool", 1),
    UniformColor=_ControlSpec("color", "float", 3),
    UniformFloat=_ControlSpec("range", "float", 1),
    UniformInt=_ControlSpec("range", "int", 1),
    UniformIVec2=_ControlSpec("vector", "int", 2),
    UniformIVec3=_ControlSpec("vector", "int", 3),
    UniformIVec4=_ControlSpec("vector", "int", 4),
    UniformMat4=_ControlSpec("vector", "float", 16),
    UniformUInt=_ControlSpec("range", "uint", 1),
    UniformUIVec2=_ControlSpec("vector", "uint", 2),
    UniformUIVec3=_ControlSpec("vector", "uint", 3),
    UniformUIVec4=_ControlSpec("vector", "uint", 4),
    UniformVec2=_ControlSpec("vector", "float", 2),
    UniformVec3=_ControlSpec("vector", "float", 3),
    UniformVec4=_ControlSpec("vector", "float", 4),
)


def get_model_data(scene) -> List[Dict[str, Any]]:
    livectls = ngl.get_livectls(scene)
    model_data = []
    for label, ctl in livectls.items():
        node_type = ctl["node_type"]
        spec = _NODE_TYPES_SPEC_MAP.get(node_type)
        if spec is None:
            print(f"live control type {node_type} is not yet supported")
            continue
        data = dict(
            type=spec.control_type,
            label=label,
            val=ctl["val"],
            node=ctl["node"],
        )

        if spec.base_type in ("int", "uint"):
            data["step"] = 1
        if data["type"] == "vector":
            data["n"] = spec.components
        if data["type"] not in ("bool", "text"):
            data["min"] = ctl["min"]
            data["max"] = ctl["max"]
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
