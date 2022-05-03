#
# Copyright 2020 GoPro Inc.
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

import colorsys
import random

from pynodegl_utils.tests.debug import get_debug_points

import pynodegl as ngl

_FIELDS_VERT = """
void main()
{
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
    var_uvcoord = ngl_uvcoord;
}
"""

_FIELDS_FRAG = """
float in_rect(vec4 rect, vec2 pos)
{
    return (1.0 - step(pos.x, rect.x)) * step(pos.x, rect.x + rect.z) *
           (1.0 - step(pos.y, rect.y)) * step(pos.y, rect.y + rect.w);
}

%(func_definitions)s

void main()
{
    float w = 1.0;
    float h = 1.0 / float(nb_fields);
    vec3 res = %(func_calls)s;
    ngl_out_color = vec4(res, 1.0);
}
"""

_ARRAY_TPL = """
vec3 get_color_%(field_name)s(float w, float h, float x, float y)
{
    float amount = 0.0;
    int len = %(field_len)d;
    for (int i = 0; i < len; i++) {
        %(amount_code)s
    }
    return %(colors_prefix)s%(field_name)s * amount;
}
"""

_SINGLE_TPL = """
vec3 get_color_%(field_name)s(float w, float h, float x, float y)
{
    float amount = 0.0;
    %(amount_code)s
    return %(colors_prefix)s%(field_name)s * amount;
}
"""

_COMMON_INT_TPL = "amount += float(%(fields_prefix)s%(field_name)s%(vec_field)s) * %(scale)f * in_rect(rect_%(comp_id)d, var_uvcoord);"
_COMMON_FLT_TPL = "amount += %(fields_prefix)s%(field_name)s%(vec_field)s * in_rect(rect_%(comp_id)d, var_uvcoord);"

_RECT_ARRAY_TPL = "vec4 rect_%(comp_id)d = vec4(x + %(row)f * w / (%(nb_rows)f * float(len)) + float(i) * w / float(len), y + %(col)f * h / %(nb_cols)f, w / %(nb_rows)f / float(len), h / %(nb_cols)f);"
_RECT_SINGLE_TPL = "vec4 rect_%(comp_id)d = vec4(x + %(col)f * w / %(nb_cols)f, y + %(row)f * h / %(nb_rows)f, w / %(nb_cols)f, h / %(nb_rows)f);"

ANIM_DURATION = 5.0
LAYOUTS = ("std140", "std430", "uniform")

# row, col, scale
_TYPE_SPEC = dict(
    # fmt: off
    bool=     (1, 1, 1.0),
    float=    (1, 1, None),
    vec2=     (1, 2, None),
    vec3=     (1, 3, None),
    vec4=     (1, 4, None),
    mat4=     (4, 4, None),
    int=      (1, 1, 1. / 255.),
    ivec2=    (1, 2, 1. / 255.),
    ivec3=    (1, 3, 1. / 255.),
    ivec4=    (1, 4, 1. / 255.),
    uint=     (1, 1, 1. / 255.),
    uvec2=    (1, 2, 1. / 255.),
    uvec3=    (1, 3, 1. / 255.),
    uvec4=    (1, 4, 1. / 255.),
    quat_mat4=(4, 4, None),
    quat_vec4=(1, 4, None),
    color=    (1, 3, None),
    # fmt: on
)


def _get_display_glsl_func(layout, field_name, field_type, field_len=None):
    rows, cols, scale = _TYPE_SPEC[field_type]
    nb_comp = rows * cols

    is_array = field_len is not None
    tpl = _ARRAY_TPL if is_array else _SINGLE_TPL
    rect_tpl = _RECT_ARRAY_TPL if is_array else _RECT_SINGLE_TPL
    amount_tpl = _COMMON_INT_TPL if scale is not None else _COMMON_FLT_TPL

    tpl_data = dict(
        colors_prefix="color_" if layout == "uniform" else "colors.",
        fields_prefix="field_" if layout == "uniform" else "fields.",
        field_name=field_name,
        field_len=field_len,
        nb_comp=nb_comp,
    )

    rect_lines = []
    amount_lines = []
    for row in range(rows):
        for col in range(cols):
            comp_id = row * cols + col

            tpl_data["col"] = col
            tpl_data["row"] = row
            tpl_data["nb_cols"] = cols
            tpl_data["nb_rows"] = rows

            if nb_comp == 16:
                tpl_data["vec_field"] = "[%d][%d]" % (col, row)
            elif nb_comp >= 1 and nb_comp <= 4:
                tpl_data["vec_field"] = "." + "xyzw"[comp_id] if nb_comp != 1 else ""
            else:
                assert False

            if is_array:
                tpl_data["vec_field"] = "[i]" + tpl_data["vec_field"]

            if scale:
                tpl_data["scale"] = scale

            tpl_data["comp_id"] = comp_id
            rect_lines.append(rect_tpl % tpl_data)
            amount_lines.append(amount_tpl % tpl_data)

    tpl_data["amount_code"] = ("\n" + (1 + is_array) * 4 * " ").join(rect_lines + amount_lines)

    return tpl % tpl_data


def _get_debug_point(rect):
    xpos = rect[0] + rect[2] / 2.0
    ypos = rect[1] + rect[3] / 2.0
    xpos = xpos * 2.0 - 1
    ypos = ypos * 2.0 - 1
    return xpos, ypos


def get_data_debug_positions(fields):
    debug_points = {}
    for i, field in enumerate(fields):
        array_len = field.get("len")
        nb_rows, nb_cols, _ = _TYPE_SPEC[field["type"]]

        name = field["name"]
        comp_id = 0

        w = 1.0
        h = 1.0 / float(len(fields))
        x = 0.0
        y = float(i) * h

        if array_len is None:
            for row in range(nb_rows):
                for col in range(nb_cols):
                    rect = (
                        x + col * w / float(nb_cols),
                        y + row * h / float(nb_rows),
                        w / float(nb_cols),
                        h / float(nb_rows),
                    )
                    debug_points[f"{name}_{comp_id}"] = _get_debug_point(rect)
                    comp_id += 1

        else:
            for i in range(array_len):
                for row in range(nb_rows):
                    for col in range(nb_cols):
                        rect = (
                            x + row * w / (nb_rows * array_len) + i * w / float(array_len),
                            y + col * h / float(nb_cols),
                            w / float(nb_rows) / float(array_len),
                            h / float(nb_cols),
                        )
                        debug_points[f"{name}_{comp_id}"] = _get_debug_point(rect)
                        comp_id += 1

    return debug_points


def match_fields(fields, category, field_type):
    target = (category, field_type)
    return [f for f in fields if (f["category"], f["type"]) == target]


def gen_floats(n):
    return [(i + 0.5) / float(n) for i in range(n)]


def gen_ints(n):
    return [int(i * 256) for i in gen_floats(n)]


def _get_anim_kf(key_cls, data):
    t0, t1, t2 = 0, ANIM_DURATION / 2.0, ANIM_DURATION
    if data is None:
        _, v1, v2, _ = gen_floats(4)
        return [key_cls(t0, v1), key_cls(t1, v2), key_cls(t2, v1)]
    return [
        key_cls(t0, data),
        key_cls(t1, data[::-1]),
        key_cls(t2, data),
    ]


_FUNCS = dict(
    # fmt: off
    animated_float=       lambda data: ngl.AnimatedFloat(keyframes=_get_anim_kf(ngl.AnimKeyFrameFloat, data)),
    animated_vec2=        lambda data: ngl.AnimatedVec2(keyframes=_get_anim_kf(ngl.AnimKeyFrameVec2, data)),
    animated_vec3=        lambda data: ngl.AnimatedVec3(keyframes=_get_anim_kf(ngl.AnimKeyFrameVec3, data)),
    animated_vec4=        lambda data: ngl.AnimatedVec4(keyframes=_get_anim_kf(ngl.AnimKeyFrameVec4, data)),
    animated_buffer_float=lambda data: ngl.AnimatedBufferFloat(keyframes=_get_anim_kf(ngl.AnimKeyFrameBuffer, data)),
    animated_buffer_vec2= lambda data: ngl.AnimatedBufferVec2(keyframes=_get_anim_kf(ngl.AnimKeyFrameBuffer, data)),
    animated_buffer_vec3= lambda data: ngl.AnimatedBufferVec3(keyframes=_get_anim_kf(ngl.AnimKeyFrameBuffer, data)),
    animated_buffer_vec4= lambda data: ngl.AnimatedBufferVec4(keyframes=_get_anim_kf(ngl.AnimKeyFrameBuffer, data)),
    animated_quat_mat4=   lambda data: ngl.AnimatedQuat(keyframes=_get_anim_kf(ngl.AnimKeyFrameQuat, data), as_mat4=True),
    animated_quat_vec4=   lambda data: ngl.AnimatedQuat(keyframes=_get_anim_kf(ngl.AnimKeyFrameQuat, data), as_mat4=False),
    array_float=          lambda data: ngl.BufferFloat(data=data),
    array_int=            lambda data: ngl.BufferInt(data=data),
    array_ivec2=          lambda data: ngl.BufferIVec2(data=data),
    array_ivec3=          lambda data: ngl.BufferIVec3(data=data),
    array_ivec4=          lambda data: ngl.BufferIVec4(data=data),
    array_mat4=           lambda data: ngl.BufferMat4(data=data),
    array_vec2=           lambda data: ngl.BufferVec2(data=data),
    array_vec3=           lambda data: ngl.BufferVec3(data=data),
    array_vec4=           lambda data: ngl.BufferVec4(data=data),
    single_bool=          lambda data: ngl.UniformBool(data),
    single_float=         lambda data: ngl.UniformFloat(data),
    single_int=           lambda data: ngl.UniformInt(data),
    single_ivec2=         lambda data: ngl.UniformIVec2(data),
    single_ivec3=         lambda data: ngl.UniformIVec3(data),
    single_ivec4=         lambda data: ngl.UniformIVec4(data),
    single_uint=          lambda data: ngl.UniformUInt(data),
    single_uvec2=         lambda data: ngl.UniformUIVec2(data),
    single_uvec3=         lambda data: ngl.UniformUIVec3(data),
    single_uvec4=         lambda data: ngl.UniformUIVec4(data),
    single_mat4=          lambda data: ngl.UniformMat4(data),
    single_quat_mat4=     lambda data: ngl.UniformQuat(data, as_mat4=True),
    single_quat_vec4=     lambda data: ngl.UniformQuat(data, as_mat4=False),
    single_vec2=          lambda data: ngl.UniformVec2(data),
    single_vec3=          lambda data: ngl.UniformVec3(data),
    single_vec4=          lambda data: ngl.UniformVec4(data),
    # fmt: on
)


def get_field_scene(cfg, spec, category, field_type, seed, debug_positions, layout, color_tint):
    """
    Build a scene testing that a given data has been properly uploaded to the
    GPU memory.

    - `spec` contains all the fields that should also be declared (either in a
      block or as uniforms). All the fields are shuffled such that the field we
      are testing is always in a random position. This makes sure that we get
      the data alignment right.
    - `category` and `field_type` are filters in the spec to select the
      field(s) from which we want to read the data
    - `seed` is used to control the fields shuffling
    - `debug_positions` controls whether there are debug circles in the scene
      in order to make sure we are reading back the data colors at the
      appropriate position
    - `layout` controls the block layout or whether we are working with uniforms
    - `color_tint` is a debug helper to give a different color for each field
      (because if they have the same data, it is hard to indiscriminate them).
    """

    cfg.aspect_ratio = (1, 1)

    # Seed only defines the random for the position of the fields
    fields_pos = random.Random(seed).sample(range(len(spec)), len(spec))

    # Always the same colors whatever the user seed
    clr_rng = random.Random(0)

    fields_info = []
    for i, field_info in enumerate(spec):
        create_func = field_info.get("func")
        if create_func is None:
            create_func = _FUNCS["{category}_{type}".format(**field_info)]
        node = create_func(field_info.get("data"))
        node.set_label(field_info["name"])
        field_info["node"] = node
        if color_tint:
            hue = clr_rng.uniform(0, 1)
            field_info["color"] = colorsys.hls_to_rgb(hue, 0.6, 1.0)
        else:
            field_info["color"] = (1, 1, 1)
        fields_info.append(field_info)

    shuf_fields = [fields_info[pos] for pos in fields_pos]
    color_fields = [(f["name"], ngl.UniformVec3(f["color"], label=f["name"])) for f in fields_info]
    block_fields = [(f["name"], f["node"]) for f in shuf_fields]
    if layout == "uniform":
        color_fields = dict(color_fields)
        block_fields = dict(block_fields)
    else:
        color_fields = ngl.Block(fields=[f for n, f in color_fields], layout=layout, label="colors_block")
        block_fields = ngl.Block(fields=[f for n, f in block_fields], layout=layout, label="fields_block")

    fields = match_fields(fields_info, category, field_type)

    func_calls = []
    func_definitions = []
    for i, field in enumerate(fields):
        field_len = field.get("len")
        func_calls.append("get_color_{}(w, h, 0.0, {:f} * h)".format(field["name"], i))
        func_definitions.append(_get_display_glsl_func(layout, field["name"], field["type"], field_len=field_len))

    frag_data = dict(
        func_definitions="\n".join(func_definitions),
        func_calls=" + ".join(func_calls),
    )

    fragment = _FIELDS_FRAG % frag_data
    vertex = _FIELDS_VERT

    program = ngl.Program(vertex=vertex, fragment=fragment)
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec2())
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    render = ngl.Render(quad, program)

    if isinstance(color_fields, dict):
        assert isinstance(block_fields, dict)
        field_names = {f["name"] for f in fields}
        d = {}
        d.update(("color_" + n, u) for (n, u) in color_fields.items() if n in field_names)
        d.update(("field_" + n, u) for (n, u) in block_fields.items() if n in field_names)
        render.update_frag_resources(**d)
    else:
        render.update_frag_resources(fields=block_fields, colors=color_fields)

    render.update_frag_resources(nb_fields=ngl.UniformInt(len(fields)))

    if debug_positions:
        debug_points = get_data_debug_positions(fields)
        dbg_circles = get_debug_points(cfg, debug_points, text_size=(0.2, 0.1))
        g = ngl.Group(children=(render, dbg_circles))
        return g

    return render
