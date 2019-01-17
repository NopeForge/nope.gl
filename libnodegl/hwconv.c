/*
 * Copyright 2018 GoPro Inc.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <string.h>

#include "hwconv.h"
#include "glincludes.h"
#include "glcontext.h"
#include "log.h"
#include "math_utils.h"
#include "memory.h"
#include "program.h"
#include "utils.h"
#include "nodes.h"

static const char * const vertex_data =
    "#version 100"                                                          "\n"
    "precision highp float;"                                                "\n"
    "attribute vec4 position;"                                              "\n"
    "uniform mat4 tex_coord_matrix;"                                        "\n"
    "varying vec2 tex_coord;"                                               "\n"
    "void main()"                                                           "\n"
    "{"                                                                     "\n"
    "    gl_Position = vec4(position.xy, 0.0, 1.0);"                        "\n"
    "    tex_coord = (tex_coord_matrix * vec4(position.zw, 0.0, 1.0)).xy;"  "\n"
    "}";

static const char * const oes_to_rgba_fragment_data =
    "#version 100"                                                          "\n"
    "#extension GL_OES_EGL_image_external : require"                        "\n"
    "precision mediump float;"                                              "\n"
    "uniform samplerExternalOES tex0;"                                      "\n"
    "varying vec2 tex_coord;"                                               "\n"
    "void main(void)"                                                       "\n"
    "{"                                                                     "\n"
    "    vec4 color = texture2D(tex0, tex_coord);"                          "\n"
    "    gl_FragColor = vec4(color.rgb, 1.0);"                              "\n"
    "}";

static const char * const nv12_to_rgba_fragment_data =
    "#version 100"                                                          "\n"
    "precision mediump float;"                                              "\n"
    "uniform sampler2D tex0;"                                               "\n"
    "uniform sampler2D tex1;"                                               "\n"
    "varying vec2 tex_coord;"                                               "\n"
    "const mat4 conv = mat4("                                               "\n"
    "    1.164,     1.164,    1.164,   0.0,"                                "\n"
    "    0.0,      -0.213,    2.112,   0.0,"                                "\n"
    "    1.787,    -0.531,    0.0,     0.0,"                                "\n"
    "   -0.96625,   0.29925, -1.12875, 1.0);"                               "\n"
    "void main(void)"                                                       "\n"
    "{"                                                                     "\n"
    "    vec3 yuv;"                                                         "\n"
    "    yuv.x = texture2D(tex0, tex_coord).r;"                             "\n"
    "    yuv.yz = texture2D(tex1, tex_coord).%s;"                           "\n"
    "    gl_FragColor = conv * vec4(yuv, 1.0);"                             "\n"
    "}";

static const char * const nv12_rectangle_to_rgba_vertex_data =
    "#version 410"                                                          "\n"
    "precision highp float;"                                                "\n"
    "uniform mat4 tex_coord_matrix;"                                        "\n"
    "uniform vec2 tex_dimensions[2];"                                       "\n"
    "in vec4 position;"                                                     "\n"
    "out vec2 tex0_coord;"                                                  "\n"
    "out vec2 tex1_coord;"                                                  "\n"
    "void main()"                                                           "\n"
    "{"                                                                     "\n"
    "    gl_Position = vec4(position.xy, 0.0, 1.0);"                        "\n"
    "    vec2 coord = (tex_coord_matrix * vec4(position.zw, 0.0, 1.0)).xy;" "\n"
    "    tex0_coord = coord * tex_dimensions[0];"                           "\n"
    "    tex1_coord = coord * tex_dimensions[1];"                           "\n"
    "}";

static const char * const nv12_rectangle_to_rgba_fragment_data =
    "#version 410"                                                          "\n"
    "uniform mediump sampler2DRect tex0;"                                   "\n"
    "uniform mediump sampler2DRect tex1;"                                   "\n"
    "in vec2 tex0_coord;"                                                   "\n"
    "in vec2 tex1_coord;"                                                   "\n"
    "out vec4 color;"                                                       "\n"
    "const mat4 conv = mat4("                                               "\n"
    "    1.164,     1.164,    1.164,   0.0,"                                "\n"
    "    0.0,      -0.213,    2.112,   0.0,"                                "\n"
    "    1.787,    -0.531,    0.0,     0.0,"                                "\n"
    "   -0.96625,   0.29925, -1.12875, 1.0);"                               "\n"
    "void main(void)"                                                       "\n"
    "{"                                                                     "\n"
    "    vec3 yuv;"                                                         "\n"
    "    yuv.x = texture(tex0, tex0_coord).r;"                              "\n"
    "    yuv.yz = texture(tex1, tex1_coord).%s;"                            "\n"
    "    color = conv * vec4(yuv, 1.0);"                                    "\n"
    "}";

static const struct hwconv_desc {
    int nb_planes;
    const char *vertex_data;
    const char *fragment_data;
} hwconv_descs[] = {
    [NGLI_TEXTURE_LAYOUT_MEDIACODEC] = {
        .nb_planes = 1,
        .vertex_data = vertex_data,
        .fragment_data = oes_to_rgba_fragment_data,
    },
    [NGLI_TEXTURE_LAYOUT_NV12] = {
        .nb_planes = 2,
        .vertex_data = vertex_data,
        .fragment_data = nv12_to_rgba_fragment_data,
    },
    [NGLI_TEXTURE_LAYOUT_NV12_RECTANGLE] = {
        .nb_planes = 2,
        .vertex_data = nv12_rectangle_to_rgba_vertex_data,
        .fragment_data = nv12_rectangle_to_rgba_fragment_data,
    },
};

int ngli_hwconv_init(struct hwconv *hwconv, struct glcontext *gl,
                     GLuint dst_texture, int dst_format, int dst_width, int dst_height,
                     enum texture_layout src_layout)
{
    hwconv->gl = gl;
    hwconv->src_layout = src_layout;

    int ret;
    if ((ret = ngli_fbo_init(&hwconv->fbo, gl, dst_width, dst_height, 0))      < 0 ||
        (ret = ngli_fbo_attach_texture(&hwconv->fbo, dst_format, dst_texture)) < 0 ||
        (ret = ngli_fbo_allocate(&hwconv->fbo))                                < 0)
        return ret;

    if (src_layout != NGLI_TEXTURE_LAYOUT_NV12 &&
        src_layout != NGLI_TEXTURE_LAYOUT_NV12_RECTANGLE &&
        src_layout != NGLI_TEXTURE_LAYOUT_MEDIACODEC) {
        LOG(ERROR, "unsupported texture layout: 0x%x", src_layout);
        return -1;
    }
    const struct hwconv_desc *desc = &hwconv_descs[src_layout];

    const char *uv = gl->version < 300 ? "ra": "rg";
    char *fragment_data = ngli_asprintf(desc->fragment_data, uv);
    if (!fragment_data)
        return -1;

    hwconv->program_id = ngli_program_load(gl, desc->vertex_data, fragment_data);
    ngli_free(fragment_data);
    if (!hwconv->program_id)
        return -1;
    ngli_glUseProgram(gl, hwconv->program_id);

    hwconv->position_location = ngli_glGetAttribLocation(gl, hwconv->program_id, "position");
    if (hwconv->position_location < 0)
        return -1;

    for (int i = 0; i < desc->nb_planes; i++) {
        char name[32];
        snprintf(name, sizeof(name), "tex%d", i);
        hwconv->texture_locations[i] = ngli_glGetUniformLocation(gl, hwconv->program_id, name);
        if (hwconv->texture_locations[i] < 0)
            return -1;
        ngli_glUniform1i(gl, hwconv->texture_locations[i], i);
    }

    hwconv->texture_matrix_location = ngli_glGetUniformLocation(gl, hwconv->program_id, "tex_coord_matrix");
    if (hwconv->texture_matrix_location < 0)
        return -1;

    hwconv->texture_dimensions_location = ngli_glGetUniformLocation(gl, hwconv->program_id, "tex_dimensions");

    static const float vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
    };
    ngli_glGenBuffers(gl, 1, &hwconv->vertices_id);
    ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, hwconv->vertices_id);
    ngli_glBufferData(gl, GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    if (gl->features & NGLI_FEATURE_VERTEX_ARRAY_OBJECT) {
        ngli_glGenVertexArrays(gl, 1, &hwconv->vao_id);
        ngli_glBindVertexArray(gl, hwconv->vao_id);

        ngli_glEnableVertexAttribArray(gl, hwconv->position_location);
        ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, hwconv->vertices_id);
        ngli_glVertexAttribPointer(gl, hwconv->position_location, 4, GL_FLOAT, GL_FALSE, 4 * 4, NULL);
    }

    return 0;
}

int ngli_hwconv_convert(struct hwconv *hwconv, const struct texture_plane *planes, const float *matrix)
{
    struct glcontext *gl = hwconv->gl;
    struct fbo *fbo = &hwconv->fbo;

    ngli_fbo_bind(fbo);

    GLint viewport[4];
    ngli_glGetIntegerv(gl, GL_VIEWPORT, viewport);
    ngli_glViewport(gl, 0, 0, fbo->width, fbo->height);
    ngli_glClear(gl, GL_COLOR_BUFFER_BIT);

    ngli_glUseProgram(gl, hwconv->program_id);
    if (gl->features & NGLI_FEATURE_VERTEX_ARRAY_OBJECT) {
        ngli_glBindVertexArray(gl, hwconv->vao_id);
    } else {
        ngli_glEnableVertexAttribArray(gl, hwconv->position_location);
        ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, hwconv->vertices_id);
        ngli_glVertexAttribPointer(gl, hwconv->position_location, 4, GL_FLOAT, GL_FALSE, 4 * 4, NULL);
    }
    const struct hwconv_desc *desc = &hwconv_descs[hwconv->src_layout];
    for (int i = 0; i < desc->nb_planes; i++) {
        ngli_glActiveTexture(gl, GL_TEXTURE0 + i);
        ngli_glBindTexture(gl, planes[i].target, planes[i].id);
    }
    if (matrix) {
        ngli_glUniformMatrix4fv(gl, hwconv->texture_matrix_location, 1, GL_FALSE, matrix);
    } else {
        NGLI_ALIGNED_MAT(id_matrix) = NGLI_MAT4_IDENTITY;
        ngli_glUniformMatrix4fv(gl, hwconv->texture_matrix_location, 1, GL_FALSE, id_matrix);
    }
    if (hwconv->texture_dimensions_location >= 0) {
        ngli_assert(desc->nb_planes <= 2);
        float dimensions[4] = {0};
        for (int i = 0; i < desc->nb_planes; i++) {
            dimensions[i*2 + 0] = planes[i].width;
            dimensions[i*2 + 1] = planes[i].height;
        }
        ngli_glUniform2fv(gl, hwconv->texture_dimensions_location, desc->nb_planes, dimensions);
    }
    ngli_glDrawArrays(gl, GL_TRIANGLE_FAN, 0, 4);
    if (!(gl->features & NGLI_FEATURE_VERTEX_ARRAY_OBJECT)) {
        ngli_glDisableVertexAttribArray(gl, hwconv->position_location);
    }

    ngli_glViewport(gl, viewport[0], viewport[1], viewport[2], viewport[3]);
    ngli_fbo_unbind(fbo);

    return 0;
}

void ngli_hwconv_reset(struct hwconv *hwconv)
{
    struct glcontext *gl = hwconv->gl;
    if (!gl)
        return;

    ngli_fbo_reset(&hwconv->fbo);

    if (gl->features & NGLI_FEATURE_VERTEX_ARRAY_OBJECT)
        ngli_glDeleteVertexArrays(gl, 1, &hwconv->vao_id);
    ngli_glDeleteProgram(gl, hwconv->program_id);
    ngli_glDeleteBuffers(gl, 1, &hwconv->vertices_id);

    memset(hwconv, 0, sizeof(*hwconv));
}
