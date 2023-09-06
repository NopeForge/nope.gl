#!/usr/bin/env python3
#
# Copyright 2023 Clément Bœsch <u pkh.me>
# Copyright 2023 Nope Project
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

import json
import sys
from textwrap import dedent


def _get_node_link(nodes, node_type):
    params = nodes[node_type]
    anchor = (params[1:] if isinstance(params, str) else node_type).lower()
    return f"[{node_type}](#{anchor})"


def _get_type_str(nodes, p) -> str:
    node_type = p["type"]

    choices = p.get("choices")
    if choices:
        type_str = f"[`{choices}`](#{choices}-choices)"
    else:
        type_str = f"[`{node_type}`](#parameter-types)"

    node_types = p.get("node_types")
    if node_types:
        type_str += " (" + ", ".join(_get_node_link(nodes, x) for x in node_types) + ")"

    return type_str


def _get_default_str(p) -> str:
    default = p.get("default")
    if p["type"] == "select":
        return f"`{default}`"
    if p["type"] == "flags":
        return f"`{default}`"
    if p["type"] in ("f32", "f64"):
        return f"`{default:g}`"
    if p["type"] == "bool":
        return "`unset`" if default < 0 else f"`{default}`"
    if p["type"] in ("i32", "u32"):
        return f"`{default}`"
    if "ivec" in p["type"]:
        return "(" + ",".join(f"`{x}`" for x in default) + ")"
    if "vec" in p["type"]:
        return "(" + ",".join(f"`{x:g}`" for x in default) + ")"
    if "mat4" in p["type"]:
        m = default
        return "(" + " ".join(f"`{m[x]:g}`,`{m[x+1]:g}`,`{m[x+2]:g}`,`{m[x+3]:g}`" for x in range(0, 16, 4)) + ")"
    if p["type"] == "str":
        return f'"{default}"' if default is not None else ""
    if p["type"] == "rational":
        num, den = default
        return f"`{num}/{den}`"
    return ""


def _get_node_params_block(nodes, name, params) -> str:
    s = f"\n## {name}\n\n"

    if not params:
        return s

    s += dedent(
        """\
        Parameter | Flags | Type | Description | Default
        --------- | ----- | ---- | ----------- | :-----:
        """
    )
    for param in params:
        name = param["name"]
        type = _get_type_str(nodes, param)
        flags = "".join(f" [`{flag}`](#parameter-flags)" for flag in param["flags"])
        desc = param["desc"]
        default = _get_default_str(param)
        s += f"`{name}` | {flags} | {type} | {desc} | {default}\n"
    s += "\n\n"
    return s


def _get_parameter_types_block(types) -> str:
    s = dedent(
        """\
        Parameter types
        ===============

        Type | Description
        ---- | -----------
        """
    )
    for ptype in types:
        s += "`{name}` | {desc}\n".format(**ptype)
    s += "\n"
    return s


def _get_parameter_flags_block() -> str:
    return dedent(
        """\
        Parameter flags
        ===============

        Marker   | Meaning
        -------- | -------
        `live`   | value is live-changeable between draw calls
        `node`   | nodes with the same data size are also allowed (e.g a `vec3` parameter can accept `AnimatedVec3`, `EvalVec3`, `NoiseVec3`, …)
        `nonull` | parameter must be set
        """
    )


def _get_parameter_choices_block(choices) -> str:
    s = dedent(
        """
        Constants for choices parameters
        ================================
        """
    )
    for choice, items in choices.items():
        s += dedent(
            f"""
            ## {choice} choices

            Constant | Description
            -------- | -----------
            """
        )
        for item in items:
            s += "`{name}` | {desc}\n".format(**item)
    return s


def _get_source(file) -> str:
    return f"**Source**: [{file}](/libnopegl/{file})\n\n"


def _get_nodes_block(nodes) -> str:
    s = ""
    for node_type, node_data in nodes.items():
        if node_type == "_Node":
            continue

        if node_type.startswith("_"):
            name = f"{node_type[1:]}*"
            s += _get_node_params_block(nodes, name, node_data["params"])
            s += _get_source(node_data["file"])
            s += f"List of `{name}` nodes:\n\n"
            s += "\n".join(f"- `{node}`" for node, info in nodes.items() if info == node_type)
            s += "\n"
        elif not isinstance(node_data, str):
            s += _get_node_params_block(nodes, node_type, node_data["params"])
            s += _get_source(node_data["file"])

    return s


def _main():
    specs = sys.argv[1]
    data = json.loads(open(specs).read())

    s = dedent(
        """\
        libnopegl
        =========
        """
    )
    s += _get_nodes_block(data["nodes"])
    s += _get_parameter_types_block(data["types"])
    s += _get_parameter_flags_block()
    s += _get_parameter_choices_block(data["choices"])

    with open(sys.argv[2], "w", encoding="utf-8") as f:
        f.write(s)


if __name__ == "__main__":
    _main()
