/*
 * Copyright 2019 GoPro Inc.
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

#include <stddef.h>
#include <string.h>

#include "memory.h"
#include "nodes.h"
#include "drawutils.h"
#include "log.h"
#include "math_utils.h"
#include "program.h"

struct text_priv {
    char *text;
    float fg_color[4];
    float bg_color[4];
    float box_corner[3];
    float box_width[3];
    float box_height[3];
    int padding;
    double font_scale;
    int valign, halign;
    int aspect_ratio[2];
    GLenum min_filter;
    GLenum mag_filter;

    struct texture texture;
    struct canvas canvas;

    GLuint vao_id;
    GLuint program_id;
    GLuint vertices_id;
    GLuint uvcoord_id;
    GLint position_location;
    GLint uvcoord_location;
    GLint texture_location;
    GLint modelview_matrix_location;
    GLint projection_matrix_location;
};

#define VALIGN_CENTER 0
#define VALIGN_TOP    1
#define VALIGN_BOTTOM 2

#define HALIGN_CENTER 0
#define HALIGN_RIGHT  1
#define HALIGN_LEFT   2

static const struct param_choices valign_choices = {
    .name = "valign",
    .consts = {
        {"center", VALIGN_CENTER, .desc=NGLI_DOCSTRING("vertically centered")},
        {"bottom", VALIGN_BOTTOM, .desc=NGLI_DOCSTRING("bottom positioned")},
        {"top",    VALIGN_TOP,    .desc=NGLI_DOCSTRING("top positioned")},
        {NULL}
    }
};

static const struct param_choices halign_choices = {
    .name = "halign",
    .consts = {
        {"center", HALIGN_CENTER, .desc=NGLI_DOCSTRING("horizontally centered")},
        {"right",  HALIGN_RIGHT,  .desc=NGLI_DOCSTRING("right positioned")},
        {"left",   HALIGN_LEFT,   .desc=NGLI_DOCSTRING("left positioned")},
        {NULL}
    }
};

#define OFFSET(x) offsetof(struct text_priv, x)
static const struct node_param text_params[] = {
    {"text",         PARAM_TYPE_STR, OFFSET(text), .flags=PARAM_FLAG_CONSTRUCTOR,
                     .desc=NGLI_DOCSTRING("text string to rasterize")},
    {"fg_color",     PARAM_TYPE_VEC4, OFFSET(fg_color), {.vec={1.0, 1.0, 1.0, 1.0}},
                     .desc=NGLI_DOCSTRING("foreground text color")},
    {"bg_color",     PARAM_TYPE_VEC4, OFFSET(bg_color), {.vec={0.0, 0.0, 0.0, 0.8}},
                     .desc=NGLI_DOCSTRING("background text color")},
    {"box_corner",   PARAM_TYPE_VEC3, OFFSET(box_corner), {.vec={-1.0, -1.0, 0.0}},
                     .desc=NGLI_DOCSTRING("origin coordinates of `box_width` and `box_height` vectors")},
    {"box_width",    PARAM_TYPE_VEC3, OFFSET(box_width), {.vec={2.0, 0.0, 0.0}},
                     .desc=NGLI_DOCSTRING("box width vector")},
    {"box_height",   PARAM_TYPE_VEC3, OFFSET(box_height), {.vec={0.0, 2.0, 0.0}},
                     .desc=NGLI_DOCSTRING("box height vector")},
    {"padding",      PARAM_TYPE_INT, OFFSET(padding), {.i64=3},
                     .desc=NGLI_DOCSTRING("pixel padding around the text")},
    {"font_scale",   PARAM_TYPE_DBL, OFFSET(font_scale), {.dbl=1.0},
                     .desc=NGLI_DOCSTRING("scaling of the font")},
    {"valign",       PARAM_TYPE_SELECT, OFFSET(valign), {.i64=VALIGN_CENTER},
                     .choices=&valign_choices,
                     .desc=NGLI_DOCSTRING("vertical alignment of the text in the box")},
    {"halign",       PARAM_TYPE_SELECT, OFFSET(halign), {.i64=HALIGN_CENTER},
                     .choices=&halign_choices,
                     .desc=NGLI_DOCSTRING("horizontal alignment of the text in the box")},
    {"aspect_ratio", PARAM_TYPE_RATIONAL, OFFSET(aspect_ratio),
                     .desc=NGLI_DOCSTRING("box aspect ratio")},
    {"min_filter",   PARAM_TYPE_SELECT, OFFSET(min_filter), {.i64=GL_LINEAR_MIPMAP_LINEAR},
                     .choices=&ngli_minfilter_choices,
                     .desc=NGLI_DOCSTRING("rasterized text texture minifying function")},
    {"mag_filter",   PARAM_TYPE_SELECT, OFFSET(mag_filter), {.i64=GL_NEAREST},
                     .choices=&ngli_magfilter_choices,
                     .desc=NGLI_DOCSTRING("rasterized text texture magnification function")},
    {NULL}
};

static void set_canvas_dimensions(struct canvas *canvas, const char *s)
{
    canvas->w = 0;
    canvas->h = NGLI_FONT_H;
    int cur_w = 0;
    for (int i = 0; s[i]; i++) {
        if (s[i] == '\n') {
            cur_w = 0;
            canvas->h += NGLI_FONT_H;
        } else {
            cur_w += NGLI_FONT_W;
            canvas->w = NGLI_MAX(canvas->w, cur_w);
        }
    }
}

static int prepare_canvas(struct text_priv *s)
{
    /* Set canvas dimensions according to text (and user padding settings) */
    set_canvas_dimensions(&s->canvas, s->text);
    s->canvas.w += 2 * s->padding;
    s->canvas.h += 2 * s->padding;

    /* Pad it to match container ratio */
    const float box_width_len  = ngli_vec3_length(s->box_width);
    const float box_height_len = ngli_vec3_length(s->box_height);
    static const int default_ar[2] = {1, 1};
    const int *ar = s->aspect_ratio[1] ? s->aspect_ratio : default_ar;
    const float box_ratio = ar[0] * box_width_len / (float)(ar[1] * box_height_len);
    const float tex_ratio = s->canvas.w / (float)s->canvas.h;
    const int aspect_padw = (tex_ratio < box_ratio ? s->canvas.h * box_ratio - s->canvas.w : 0);
    const int aspect_padh = (tex_ratio < box_ratio ? 0 : s->canvas.w / box_ratio - s->canvas.h);

    /* Adjust canvas size to impact text size */
    const int texw = (s->canvas.w + aspect_padw) / s->font_scale;
    const int texh = (s->canvas.h + aspect_padh) / s->font_scale;
    const int padw = texw - s->canvas.w;
    const int padh = texh - s->canvas.h;
    s->canvas.w = NGLI_MAX(1, texw);
    s->canvas.h = NGLI_MAX(1, texh);

    /* Adjust text position according to alignment settings */
    const int tx = (s->halign == HALIGN_CENTER ? padw / 2 :
                    s->halign == HALIGN_RIGHT  ? padw     :
                    0) + s->padding;
    const int ty = (s->valign == VALIGN_CENTER ? padh / 2 :
                    s->valign == VALIGN_BOTTOM ? padh     :
                    0) + s->padding;

    /* Allocate, draw background, print text */
    s->canvas.buf = ngli_calloc(s->canvas.w * s->canvas.h, sizeof(*s->canvas.buf) * 4);
    if (!s->canvas.buf)
        return -1;
    const uint32_t fg = NGLI_COLOR_VEC4_TO_U32(s->fg_color);
    const uint32_t bg = NGLI_COLOR_VEC4_TO_U32(s->bg_color);
    struct rect rect = {.w = s->canvas.w, .h = s->canvas.h};
    ngli_drawutils_draw_rect(&s->canvas, &rect, bg);
    ngli_drawutils_print(&s->canvas, tx, ty, s->text, fg);
    return 0;
}

static const char * const vertex_data =
    "#version 100"                                                          "\n"
    "precision highp float;"                                                "\n"
    "attribute vec4 position;"                                              "\n"
    "attribute vec2 uvcoord;"                                               "\n"
    "uniform mat4 modelview_matrix;"                                        "\n"
    "uniform mat4 projection_matrix;"                                       "\n"
    "varying vec2 var_tex_coord;"                                           "\n"
    "void main()"                                                           "\n"
    "{"                                                                     "\n"
    "    gl_Position = projection_matrix * modelview_matrix * position;"    "\n"
    "    var_tex_coord = uvcoord;"                                          "\n"
    "}";

static const char * const fragment_data =
    "#version 100"                                                          "\n"
    "precision highp float;"                                                "\n"
    "uniform sampler2D tex;"                                                "\n"
    "varying vec2 var_tex_coord;"                                           "\n"
    "void main(void)"                                                       "\n"
    "{"                                                                     "\n"
    "    gl_FragColor = texture2D(tex, var_tex_coord);"                     "\n"
    "}";

static void enable_vertex_attribs(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct text_priv *s = node->priv_data;

    ngli_glEnableVertexAttribArray(gl, s->position_location);
    ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, s->vertices_id);
    ngli_glVertexAttribPointer(gl, s->position_location, 3, GL_FLOAT, GL_FALSE, 3 * 4, NULL);

    ngli_glEnableVertexAttribArray(gl, s->uvcoord_location);
    ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, s->uvcoord_id);
    ngli_glVertexAttribPointer(gl, s->uvcoord_location, 2, GL_FLOAT, GL_FALSE, 2 * 4, NULL);
}

#define C(index) s->box_corner[index]
#define W(index) s->box_width[index]
#define H(index) s->box_height[index]

static int text_init(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct text_priv *s = node->priv_data;

    int ret = prepare_canvas(s);
    if (ret < 0)
        return ret;

    const float vertices[] = {
        C(0),               C(1),               C(2),
        C(0) + W(0),        C(1) + W(1),        C(2) + W(2),
        C(0) + H(0) + W(0), C(1) + H(1) + W(1), C(2) + H(2) + W(2),
        C(0) + H(0),        C(1) + H(1),        C(2) + H(2),
    };

    static const float uvs[] = {0.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0};

    s->program_id = ngli_program_load(gl, vertex_data, fragment_data);
    if (!s->program_id)
        return -1;

    s->position_location = ngli_glGetAttribLocation(gl, s->program_id, "position");
    s->uvcoord_location  = ngli_glGetAttribLocation(gl, s->program_id, "uvcoord");
    s->texture_location  = ngli_glGetUniformLocation(gl, s->program_id, "tex");

    s->modelview_matrix_location  = ngli_glGetUniformLocation(gl, s->program_id, "modelview_matrix");
    s->projection_matrix_location = ngli_glGetUniformLocation(gl, s->program_id, "projection_matrix");

    if (s->position_location          < 0 ||
        s->uvcoord_location           < 0 ||
        s->texture_location           < 0 ||
        s->modelview_matrix_location  < 0 ||
        s->projection_matrix_location < 0)
        return -1;

    ngli_glUseProgram(gl, s->program_id);

    ngli_glUniform1i(gl, s->texture_location, 0);

    ngli_glGenBuffers(gl, 1, &s->vertices_id);
    ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, s->vertices_id);
    ngli_glBufferData(gl, GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    ngli_glGenBuffers(gl, 1, &s->uvcoord_id);
    ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, s->uvcoord_id);
    ngli_glBufferData(gl, GL_ARRAY_BUFFER, sizeof(uvs), uvs, GL_STATIC_DRAW);

    if (gl->features & NGLI_FEATURE_VERTEX_ARRAY_OBJECT) {
        ngli_glGenVertexArrays(gl, 1, &s->vao_id);
        ngli_glBindVertexArray(gl, s->vao_id);
        enable_vertex_attribs(node);
    }

    struct texture_params tex_params = NGLI_TEXTURE_PARAM_DEFAULTS;
    tex_params.width = s->canvas.w;
    tex_params.height = s->canvas.h;
    tex_params.format = NGLI_FORMAT_R8G8B8A8_UNORM;
    tex_params.min_filter = s->min_filter;
    tex_params.mag_filter = s->mag_filter;
    ret = ngli_texture_init(&s->texture, gl, &tex_params);
    if (ret < 0)
        return ret;

    return ngli_texture_upload(&s->texture, s->canvas.buf, 0);
}

static void text_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct text_priv *s = node->priv_data;

    const float *modelview_matrix  = ngli_darray_tail(&ctx->modelview_matrix_stack);
    const float *projection_matrix = ngli_darray_tail(&ctx->projection_matrix_stack);

    ngli_honor_pending_glstate(ctx);

    ngli_glUseProgram(gl, s->program_id);
    if (gl->features & NGLI_FEATURE_VERTEX_ARRAY_OBJECT)
        ngli_glBindVertexArray(gl, s->vao_id);
    else
        enable_vertex_attribs(node);

    ngli_glUniformMatrix4fv(gl, s->modelview_matrix_location, 1, GL_FALSE, modelview_matrix);
    ngli_glUniformMatrix4fv(gl, s->projection_matrix_location, 1, GL_FALSE, projection_matrix);
    ngli_glActiveTexture(gl, GL_TEXTURE0);
    ngli_glBindTexture(gl, s->texture.target, s->texture.id);
    ngli_glDrawArrays(gl, GL_TRIANGLE_FAN, 0, 4);

    if (!(gl->features & NGLI_FEATURE_VERTEX_ARRAY_OBJECT)) {
        ngli_glDisableVertexAttribArray(gl, s->position_location);
        ngli_glDisableVertexAttribArray(gl, s->uvcoord_location);
    }
}

static void text_uninit(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct text_priv *s = node->priv_data;

    if (gl->features & NGLI_FEATURE_VERTEX_ARRAY_OBJECT)
        ngli_glDeleteVertexArrays(gl, 1, &s->vao_id);
    ngli_glDeleteProgram(gl, s->program_id);
    ngli_glDeleteBuffers(gl, 1, &s->vertices_id);
    ngli_glDeleteBuffers(gl, 1, &s->uvcoord_id);
    ngli_texture_reset(&s->texture);
    ngli_free(s->canvas.buf);
}

const struct node_class ngli_text_class = {
    .id        = NGL_NODE_TEXT,
    .name      = "Text",
    .init      = text_init,
    .draw      = text_draw,
    .uninit    = text_uninit,
    .priv_size = sizeof(struct text_priv),
    .params    = text_params,
    .file      = __FILE__,
};
