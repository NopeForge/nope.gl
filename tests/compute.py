#
# Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
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

import array

from pynopegl_utils.misc import SceneCfg, get_shader, scene
from pynopegl_utils.tests.cmp_cuepoints import test_cuepoints
from pynopegl_utils.tests.cmp_fingerprint import test_fingerprint
from pynopegl_utils.tests.debug import get_debug_points
from pynopegl_utils.toolbox.colors import COLORS

import pynopegl as ngl

_PARTICULES_COMPUTE = """
void main()
{
    uvec3 total_size = gl_WorkGroupSize * gl_NumWorkGroups;
    uint i = gl_GlobalInvocationID.z * total_size.x * total_size.y
           + gl_GlobalInvocationID.y * total_size.x
           + gl_GlobalInvocationID.x;
    vec3 iposition = idata.positions[i];
    vec2 ivelocity = idata.velocities[i];
    vec3 position;
    position.x = iposition.x + time * ivelocity.x;
    position.y = iposition.y + 0.1 * sin(time * duration * ivelocity.y);
    position.z = 0.0;
    odata.positions[i] = position;
}
"""


_PARTICULES_VERT = """
void main()
{
    vec4 position = vec4(ngl_position, 1.0) + vec4(data.positions[ngl_instance_index], 0.0);
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * position;
}
"""


@test_fingerprint(keyframes=10, tolerance=1)
@scene()
def compute_particles(cfg: SceneCfg):
    cfg.duration = 10
    workgroups = (2, 1, 4)
    local_size = (4, 4, 1)
    nb_particles = workgroups[0] * workgroups[1] * workgroups[2] * local_size[0] * local_size[1] * local_size[2]

    positions = array.array("f")
    velocities = array.array("f")
    for _ in range(nb_particles):
        positions.extend(
            [
                cfg.rng.uniform(-2.0, 1.0),
                cfg.rng.uniform(-1.0, 1.0),
                0.0,
            ]
        )
        velocities.extend(
            [
                cfg.rng.uniform(1.0, 2.0),
                cfg.rng.uniform(0.5, 1.5),
            ]
        )

    ipositions = ngl.Block(
        fields=[
            ngl.BufferVec3(data=positions, label="positions"),
            ngl.BufferVec2(data=velocities, label="velocities"),
        ],
        layout="std430",
    )
    opositions = ngl.Block(fields=[ngl.BufferVec3(count=nb_particles, label="positions")], layout="std140")

    animkf = [
        ngl.AnimKeyFrameFloat(0, 0),
        ngl.AnimKeyFrameFloat(cfg.duration, 1.0),
    ]
    time = ngl.AnimatedFloat(animkf)
    duration = ngl.UniformFloat(cfg.duration)

    program = ngl.ComputeProgram(_PARTICULES_COMPUTE, workgroup_size=local_size)
    program.update_properties(odata=ngl.ResourceProps(writable=True))
    compute = ngl.Compute(workgroups, program)
    compute.update_resources(time=time, duration=duration, idata=ipositions, odata=opositions)

    circle = ngl.Circle(radius=0.05)
    program = ngl.Program(vertex=_PARTICULES_VERT, fragment=get_shader("color.frag"))
    render = ngl.Render(circle, program, nb_instances=nb_particles)
    render.update_frag_resources(color=ngl.UniformVec3(value=COLORS.sgreen), opacity=ngl.UniformFloat(1))
    render.update_vert_resources(data=opositions)

    group = ngl.Group()
    group.add_children(compute, render)
    return group


_COMPUTE_HISTOGRAM_CLEAR = """
void main()
{
    uint i = gl_GlobalInvocationID.x;
    atomicAnd(hist.r[i], 0U);
    atomicAnd(hist.g[i], 0U);
    atomicAnd(hist.b[i], 0U);
    atomicAnd(hist.max.r, 0U);
    atomicAnd(hist.max.g, 0U);
    atomicAnd(hist.max.b, 0U);
}
"""


_COMPUTE_HISTOGRAM_EXEC = """
void main()
{
    uint x = gl_GlobalInvocationID.x;
    uint y = gl_GlobalInvocationID.y;

    ivec2 size = imageSize(source);
    if (x < uint(size.x) && y < uint(size.y)) {
        vec4 color = imageLoad(source, ivec2(x, y));
        uvec4 ucolor = uvec4(color * (%(hsize)d.0 - 1.0));
        uint r = atomicAdd(hist.r[ucolor.r], 1U);
        uint g = atomicAdd(hist.g[ucolor.g], 1U);
        uint b = atomicAdd(hist.b[ucolor.b], 1U);
        atomicMax(hist.max.r, r);
        atomicMax(hist.max.g, g);
        atomicMax(hist.max.b, b);
    }
}
"""


_RENDER_HISTOGRAM_VERT = """
void main()
{
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
    var_uvcoord = ngl_uvcoord;
}
"""


_RENDER_HISTOGRAM_FRAG = """
void main()
{
    uint x = uint(var_uvcoord.x * %(size)d.0);
    uint y = uint(var_uvcoord.y * %(size)d.0);
    uint i = clamp(x + y * %(size)dU, 0U, %(hsize)dU - 1U);
    vec3 rgb = vec3(hist.r[i], hist.g[i], hist.b[i]) / vec3(hist.max);
    ngl_out_color = vec4(rgb, 1.0);
}
"""


_N = 8


def _get_compute_histogram_cuepoints():
    f = float(_N)
    off = 1 / (2 * f)
    c = lambda i: (i / f + off) * 2.0 - 1.0
    return {"%d%d" % (x, y): (c(x), c(y)) for y in range(_N) for x in range(_N)}


@test_cuepoints(points=_get_compute_histogram_cuepoints(), tolerance=1)
@scene(controls=dict(show_dbg_points=scene.Bool()))
def compute_histogram(cfg: SceneCfg, show_dbg_points=False):
    cfg.duration = 10
    cfg.aspect_ratio = (1, 1)
    hsize, size, local_size = _N * _N, _N, _N // 2
    data = array.array("f")
    for _ in range(size * size):
        data.extend(
            (
                cfg.rng.uniform(0.0, 0.5),
                cfg.rng.uniform(0.25, 0.75),
                cfg.rng.uniform(0.5, 1.0),
                1.0,
            )
        )
    texture_buffer = ngl.BufferVec4(data=data)
    texture = ngl.Texture2D(
        width=size,
        height=size,
        data_src=texture_buffer,
        min_filter="nearest",
        mag_filter="nearest",
    )
    texture.set_format("r32g32b32a32_sfloat")

    histogram_block = ngl.Block(layout="std140", label="histogram")
    histogram_block.add_fields(
        ngl.BufferUInt(hsize, label="r"),
        ngl.BufferUInt(hsize, label="g"),
        ngl.BufferUInt(hsize, label="b"),
        ngl.UniformUIVec3(label="max"),
    )

    shader_params = dict(hsize=hsize, size=size, local_size=local_size)

    group_size = hsize // local_size
    clear_histogram_shader = _COMPUTE_HISTOGRAM_CLEAR % shader_params
    clear_histogram_program = ngl.ComputeProgram(clear_histogram_shader, workgroup_size=(local_size, 1, 1))
    clear_histogram_program.update_properties(hist=ngl.ResourceProps(writable=True))
    clear_histogram = ngl.Compute(
        workgroup_count=(group_size, 1, 1),
        program=clear_histogram_program,
        label="clear_histogram",
    )
    clear_histogram.update_resources(hist=histogram_block)

    group_size = size // local_size
    exec_histogram_shader = _COMPUTE_HISTOGRAM_EXEC % shader_params
    exec_histogram_program = ngl.ComputeProgram(exec_histogram_shader, workgroup_size=(local_size, local_size, 1))
    exec_histogram_program.update_properties(hist=ngl.ResourceProps(writable=True))
    exec_histogram = ngl.Compute(
        workgroup_count=(group_size, group_size, 1), program=exec_histogram_program, label="compute_histogram"
    )
    exec_histogram.update_resources(hist=histogram_block, source=texture)
    exec_histogram_program.update_properties(source=ngl.ResourceProps(as_image=True))

    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    program = ngl.Program(
        vertex=_RENDER_HISTOGRAM_VERT,
        fragment=_RENDER_HISTOGRAM_FRAG % shader_params,
    )
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec2())
    render = ngl.Render(quad, program, label="render_histogram")
    render.update_frag_resources(hist=histogram_block)

    group = ngl.Group(children=(clear_histogram, exec_histogram, render))
    if show_dbg_points:
        cuepoints = _get_compute_histogram_cuepoints()
        group.add_children(get_debug_points(cfg, cuepoints))
    return group


_ANIMATION_COMPUTE = """
void main()
{
    uint i = gl_WorkGroupID.x * gl_WorkGroupSize.x * gl_WorkGroupSize.y + gl_LocalInvocationIndex;
    dst.vertices[i] = vec3(transform * vec4(src.vertices[i], 1.0));
}
"""


def _compute_animation(cfg: SceneCfg, animate_pre_render=True):
    cfg.duration = 5
    cfg.aspect_ratio = (1, 1)
    local_size = 2

    vertices_data = array.array(
        "f",
        [
            # fmt: off
            -0.5, -0.5, 0.0,
             0.5, -0.5, 0.0,
            -0.5,  0.5, 0.0,
             0.5,  0.5, 0.0,
            # fmt: on
        ],
    )
    nb_vertices = 4

    input_vertices = ngl.BufferVec3(data=vertices_data, label="vertices")
    output_vertices = ngl.BufferVec3(data=vertices_data, label="vertices")
    input_padding = ngl.UniformVec3(label="padding")
    output_padding = ngl.UniformVec4(label="padding")
    input_block = ngl.Block(fields=[input_padding, input_vertices], layout="std140")
    output_block = ngl.Block(fields=[output_padding, output_vertices], layout="std140")

    rotate_animkf = [ngl.AnimKeyFrameFloat(0, 0), ngl.AnimKeyFrameFloat(cfg.duration, 360)]
    rotate = ngl.Rotate(ngl.Identity(), axis=(0, 0, 1), angle=ngl.AnimatedFloat(rotate_animkf))
    transform = ngl.UniformMat4(transform=rotate)

    program = ngl.ComputeProgram(_ANIMATION_COMPUTE, workgroup_size=(local_size, local_size, 1))
    program.update_properties(dst=ngl.ResourceProps(writable=True))
    compute = ngl.Compute(workgroup_count=(nb_vertices // (local_size**2), 1, 1), program=program)
    compute.update_resources(transform=transform, src=input_block, dst=output_block)

    quad_buffer = ngl.BufferVec3(block=output_block, block_field="vertices")
    geometry = ngl.Geometry(quad_buffer, topology="triangle_strip")
    program = ngl.Program(vertex=get_shader("color.vert"), fragment=get_shader("color.frag"))
    render = ngl.Render(geometry, program)
    render.update_frag_resources(color=ngl.UniformVec3(value=COLORS.sgreen), opacity=ngl.UniformFloat(1))

    children = (compute, render) if animate_pre_render else (render, compute)
    return ngl.Group(children=children)


@test_fingerprint(keyframes=5, tolerance=1)
@scene()
def compute_animation(cfg: SceneCfg):
    return _compute_animation(cfg)


@test_fingerprint(keyframes=5, tolerance=1)
@scene()
def compute_animation_post_render(cfg: SceneCfg):
    return _compute_animation(cfg, False)


_IMAGE_LOAD_STORE_COMPUTE = """
void main()
{
    ivec2 pos = ivec2(gl_LocalInvocationID.xy);
    vec4 color;
    color.r = imageLoad(texture_r, pos).r;
    color.g = imageLoad(texture_g, pos).r;
    color.b = imageLoad(texture_b, pos).r;
    color.a = 1.0;
    color.rgb = (color.rgb * scale.factors.x) + scale.factors.y;
    imageStore(texture_rgba, pos, color);
}
"""


@test_cuepoints(points=_get_compute_histogram_cuepoints(), tolerance=1)
@scene(controls=dict(show_dbg_points=scene.Bool()))
def compute_image_load_store(cfg: SceneCfg, show_dbg_points=False):
    size = _N
    texture_data = ngl.BufferFloat(data=array.array("f", [x / (size**2) for x in range(size**2)]))
    texture_r = ngl.Texture2D(
        format="r32_sfloat",
        width=size,
        height=size,
        data_src=texture_data,
        min_filter="nearest",
        mag_filter="nearest",
    )
    texture_g = ngl.Texture2D(
        format="r32_sfloat",
        width=size,
        height=size,
        data_src=texture_data,
        min_filter="nearest",
        mag_filter="nearest",
    )
    texture_b = ngl.Texture2D(
        format="r32_sfloat",
        width=size,
        height=size,
        data_src=texture_data,
        min_filter="nearest",
        mag_filter="nearest",
    )
    scale = ngl.Block(
        fields=[ngl.UniformVec2(value=(-1.0, 1.0), label="factors")],
        layout="std140",
    )
    texture_rgba = ngl.Texture2D(
        width=size,
        height=size,
        min_filter="nearest",
        mag_filter="nearest",
    )
    program = ngl.ComputeProgram(_IMAGE_LOAD_STORE_COMPUTE, workgroup_size=(size, size, 1))
    program.update_properties(
        texture_r=ngl.ResourceProps(as_image=True),
        texture_g=ngl.ResourceProps(as_image=True),
        texture_b=ngl.ResourceProps(as_image=True),
        texture_rgba=ngl.ResourceProps(as_image=True, writable=True),
    )
    compute = ngl.Compute(workgroup_count=(1, 1, 1), program=program)
    compute.update_resources(
        texture_r=texture_r, texture_g=texture_g, texture_b=texture_b, scale=scale, texture_rgba=texture_rgba
    )

    render = ngl.RenderTexture(texture_rgba)
    group = ngl.Group(children=(compute, render))

    if show_dbg_points:
        cuepoints = _get_compute_histogram_cuepoints()
        group.add_children(get_debug_points(cfg, cuepoints))

    return group


_IMAGE_LAYERED_STORE_COMPUTE = """
void main()
{
    ivec2 pos = ivec2(gl_LocalInvocationID.xy);
    float size = float(imageSize(texture).x);
    float color = (float(pos.y) * size + float(pos.x)) / (size * size);
    imageStore(texture, ivec3(pos, 0), vec4(color));
    imageStore(texture, ivec3(pos, 1), vec4(color));
    imageStore(texture, ivec3(pos, 2), vec4(color));
}
"""

_IMAGE_LAYERED_LOAD_STORE_COMPUTE = """
void main()
{
    ivec2 pos = ivec2(gl_LocalInvocationID.xy);
    vec4 color;
    color.r = imageLoad(texture, ivec3(pos, 0)).r;
    color.g = imageLoad(texture, ivec3(pos, 1)).r;
    color.b = imageLoad(texture, ivec3(pos, 2)).r;
    color.a = 1.0;
    color.rgb = color.rgb * scale.factors.x + scale.factors.y;
    imageStore(texture_rgba, pos, color);
}
"""


def _get_image_layered_load_store_cuepoints():
    f = float(_N)
    off = 1 / (2 * f)
    c = lambda i: (i / f + off) * 2.0 - 1.0
    return {f"{x}{y}": (c(x), c(y)) for y in range(_N) for x in range(_N)}


def _get_compute_image_layered_load_store_scene(cfg: SceneCfg, texture_cls, show_dbg_points=False):
    size = _N
    texture = texture_cls(
        format="r32_sfloat",
        width=size,
        height=size,
        depth=3,
        min_filter="nearest",
        mag_filter="nearest",
    )
    texture_rgba = ngl.Texture2D(width=size, height=size, min_filter="nearest", mag_filter="nearest")
    program_store = ngl.ComputeProgram(_IMAGE_LAYERED_STORE_COMPUTE, workgroup_size=(size, size, 1))
    program_store.update_properties(
        texture=ngl.ResourceProps(as_image=True, writable=True),
    )
    compute_store = ngl.Compute(workgroup_count=(1, 1, 1), program=program_store)
    compute_store.update_resources(
        texture=texture,
    )

    # Each color component is stored in a separate layer of the 3D texture.
    # This second compute pass load them, invert them using the scale factors
    # declared in a block (to mix bindings of different type) and store them in a
    # RGBA texture.
    scale = ngl.Block(
        fields=[ngl.UniformVec2(value=(-1.0, 1.0), label="factors")],
        layout="std140",
    )
    program_load_store = ngl.ComputeProgram(_IMAGE_LAYERED_LOAD_STORE_COMPUTE, workgroup_size=(size, size, 1))
    program_load_store.update_properties(
        texture=ngl.ResourceProps(as_image=True),
        texture_rgba=ngl.ResourceProps(as_image=True, writable=True),
    )
    compute_load_store = ngl.Compute(workgroup_count=(1, 1, 1), program=program_load_store)
    compute_load_store.update_resources(
        texture=texture,
        texture_rgba=texture_rgba,
        scale=scale,
    )

    render = ngl.RenderTexture(texture_rgba)
    group = ngl.Group(children=(compute_store, compute_load_store, render))

    if show_dbg_points:
        cuepoints = _get_image_layered_load_store_cuepoints()
        group.add_children(get_debug_points(cfg, cuepoints))

    return group


@test_cuepoints(points=_get_image_layered_load_store_cuepoints(), tolerance=1)
@scene(controls=dict(show_dbg_points=scene.Bool()))
def compute_image_3d_load_store(cfg: SceneCfg, show_dbg_points=False):
    return _get_compute_image_layered_load_store_scene(cfg, ngl.Texture3D, show_dbg_points)


@test_cuepoints(points=_get_image_layered_load_store_cuepoints(), tolerance=1)
@scene(controls=dict(show_dbg_points=scene.Bool()))
def compute_image_2d_array_load_store(cfg: SceneCfg, show_dbg_points=False):
    return _get_compute_image_layered_load_store_scene(cfg, ngl.Texture2DArray, show_dbg_points)


_IMAGE_CUBE_STORE_COMPUTE = """
void main()
{
    ivec3 pos = ivec3(gl_LocalInvocationID.xyz);
    vec4 color = vec4(0.0);
    if (pos.z == 0) // right
        color = vec4(1.0, 0.0, 0.0, 1.0);
    else if (pos.z == 1) // left
        color = vec4(0.0, 1.0, 0.0, 1.0);
    else if (pos.z == 2) // top
        color = vec4(0.0, 0.0, 1.0, 1.0);
    else if (pos.z == 3) // bottom
        color = vec4(1.0, 1.0, 0.0, 1.0);
    else if (pos.z == 4) // back
        color = vec4(0.0, 1.0, 1.0, 1.0);
    else if (pos.z == 5) // front
        color = vec4(1.0, 0.0, 1.0, 1.0);
    imageStore(texture, pos, color);
}
"""

_IMAGE_CUBE_VERT = """
void main()
{
    ngl_out_pos = ngl_projection_matrix * ngl_modelview_matrix * vec4(ngl_position, 1.0);
    var_uvcoord = ngl_position;
}
"""

_IMAGE_CUBE_FRAG = """
// Cube map texture selection is described in the OpenGL specification, see:
// https://registry.khronos.org/OpenGL/specs/es/3.0/es_spec_3.0.pdf#table.3.21
vec2 get_sample_cube_coord(vec3 r, out int face)
{
    vec3 r_abs = abs(r);
    vec2 uv;  // sc,tc
    float ma; // major axis
    if (r_abs.z >= r_abs.x && r_abs.z >= r_abs.y) { // z major
        uv = vec2(sign(r.z) * r.x, -r.y);
        ma = r_abs.z;
        face = r.z < 0.0 ? 5 : 4;
    } else if (r_abs.y >= r_abs.x) { // y major
        uv = vec2(r.x, sign(r.y) * r.z);
        ma = r_abs.y;
        face = r.y < 0.0 ? 3 : 2;
    } else { // x major
        uv = vec2(sign(r.x) * -r.z, -r.y);
        ma = r_abs.x;
        face = r.x < 0.0 ? 1 : 0;
    }
    return (uv / ma + 1.0) / 2.0;
}

void main()
{
    int face = 0;
    vec2 uv = get_sample_cube_coord(vec3(var_uvcoord.xy, 0.5), face);
    float size = float(imageSize(texture).x);
    ngl_out_color = imageLoad(texture, ivec3(uv * size, face));
}
"""


@test_fingerprint()
@scene()
def compute_image_cube_load_store(cfg: SceneCfg):
    size = _N
    texture = ngl.TextureCube(format="r32g32b32a32_sfloat", size=size, min_filter="nearest", mag_filter="nearest")
    program_store = ngl.ComputeProgram(_IMAGE_CUBE_STORE_COMPUTE, workgroup_size=(size, size, 6))
    program_store.update_properties(
        texture=ngl.ResourceProps(as_image=True, writable=True),
    )
    compute_store = ngl.Compute(workgroup_count=(1, 1, 1), program=program_store)
    compute_store.update_resources(
        texture=texture,
    )

    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    program = ngl.Program(vertex=_IMAGE_CUBE_VERT, fragment=_IMAGE_CUBE_FRAG)
    program.update_vert_out_vars(var_uvcoord=ngl.IOVec3())
    program.update_properties(
        texture=ngl.ResourceProps(as_image=True),
    )
    render = ngl.Render(quad, program)
    render.update_frag_resources(texture=texture)

    return ngl.Group(children=(compute_store, render))
