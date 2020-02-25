#!/usr/bin/env python
# -*- coding: utf-8 -*-
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

import array
import random
import pynodegl as ngl
from pynodegl_utils.misc import scene
from pynodegl_utils.toolbox.colors import COLORS
from pynodegl_utils.tests.cmp_fingerprint import test_fingerprint


_PARTICULES_COMPUTE = '''
#version %(version)s

layout(local_size_x = %(local_size)d, local_size_y = %(local_size)d, local_size_z = 1) in;

layout (std430, binding = 0) buffer ipositions_buffer {
    vec3 ipositions[%(nb_particules)d];
    vec2 ivelocities[%(nb_particules)d];
};

layout (std430, binding = 1) buffer opositions_buffer {
    vec3 opositions[%(nb_particules)d];
};

uniform float time;
uniform float duration;

void main(void)
{
    uint i = gl_WorkGroupID.x * gl_WorkGroupSize.x * gl_WorkGroupSize.y + gl_LocalInvocationIndex;
    vec3 iposition = ipositions[i];
    vec2 ivelocity = ivelocities[i];
    vec3 position;
    position.x = iposition.x + time * ivelocity.x;
    position.y = iposition.y + 0.1 * sin(time * duration * ivelocity.y);
    position.z = 0.0;
    opositions[i] = position;
}
'''


_PARTICULES_VERT = '''
#version %(version)s
precision highp float;
in vec4 ngl_position;
uniform mat4 ngl_modelview_matrix;
uniform mat4 ngl_projection_matrix;

layout(std430, binding = 0) buffer positions_buffer {
    vec3 positions[];
};

void main(void)
{
    vec4 position = ngl_position + vec4(positions[gl_InstanceID], 0.0);
    gl_Position = ngl_projection_matrix * ngl_modelview_matrix * position;
}
'''


_PARTICULES_FRAG = '''
#version %(version)s
precision mediump float;
uniform vec4 color;
out vec4 frag_color;

void main(void)
{
    frag_color = color;
}
'''


@test_fingerprint(nb_keyframes=10)
@scene()
def compute_particules(cfg):
    random.seed(0)
    cfg.duration = 10
    local_size = 4
    nb_particules = 128

    shader_version = '310 es' if cfg.backend == 'gles' else '430'
    shader_data = dict(
        version=shader_version,
        local_size=local_size,
        nb_particules=nb_particules,
    )
    compute_shader = _PARTICULES_COMPUTE % shader_data
    vertex_shader = _PARTICULES_VERT % shader_data
    fragment_shader = _PARTICULES_FRAG % shader_data

    positions = array.array('f')
    velocities = array.array('f')
    for i in range(nb_particules):
        positions.extend([
            random.uniform(-2.0, 1.0),
            random.uniform(-1.0, 1.0),
            0.0,
        ])
        velocities.extend([
            random.uniform(1.0, 2.0),
            random.uniform(0.5, 1.5),
        ])

    ipositions = ngl.Block(
        fields=[
            ngl.BufferVec3(data=positions),
            ngl.BufferVec2(data=velocities),
        ],
        layout='std430',
    )
    opositions = ngl.Block(fields=[ngl.BufferVec3(count=nb_particules)], layout='std430')

    animkf = [
        ngl.AnimKeyFrameFloat(0, 0),
        ngl.AnimKeyFrameFloat(cfg.duration, 1.0),
    ]
    time = ngl.AnimatedFloat(animkf)
    duration = ngl.UniformFloat(cfg.duration)

    group_size = nb_particules / local_size
    program = ngl.ComputeProgram(compute_shader)
    compute = ngl.Compute(nb_particules, 1, 1, program)
    compute.update_uniforms(time=time, duration=duration)
    compute.update_blocks(ipositions_buffer=ipositions, opositions_buffer=opositions)

    circle = ngl.Circle(radius=0.05)
    program = ngl.Program(vertex=vertex_shader, fragment=fragment_shader)
    render = ngl.Render(circle, program, nb_instances=nb_particules)
    render.update_uniforms(color=ngl.UniformVec4(value=COLORS['sgreen']))
    render.update_blocks(positions_buffer=opositions)

    group = ngl.Group()
    group.add_children(compute, render)
    return group
