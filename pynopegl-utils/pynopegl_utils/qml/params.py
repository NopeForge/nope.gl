#
# Copyright 2023 Clément Bœsch <u pkh.me>
# Copyright 2023 Nope Foundry
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

# Convert from scene namedtuple to role value in the QML model
_WIDGET_TYPES_MODEL_MAP = dict(
    Range="range",
    Vector="vector",
    Color="color",
    Bool="bool",
    File="file",
    List="list",
    Text="text",
)


def get_model_data(widgets_specs) -> List[Dict[str, Any]]:
    model_data = []
    for key, default, ctl_id, ctl_data in widgets_specs:
        type_ = _WIDGET_TYPES_MODEL_MAP.get(ctl_id)
        if type_ is None:
            print(f"widget type {ctl_id} is not yet supported")
            continue

        data = dict(type=type_, label=key, val=default)
        if ctl_id == "Range":
            data["min"] = ctl_data["range"][0]
            data["max"] = ctl_data["range"][1]
            data["step"] = 1 / ctl_data["unit_base"]
        elif ctl_id == "Vector":
            n = ctl_data["n"]
            minv = ctl_data.get("minv")
            maxv = ctl_data.get("maxv")
            data["n"] = n
            data["min"] = [0] * n if minv is None else list(minv)
            data["val"] = list(default)
            data["max"] = [1] * n if maxv is None else list(maxv)
        elif ctl_id == "List":
            data["choices"] = list(ctl_data["choices"])
        elif ctl_id == "File":
            data["filter"] = ctl_data["filter"]

        model_data.append(data)

    return model_data
