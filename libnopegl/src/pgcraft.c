/*
 * Copyright 2020-2022 GoPro Inc.
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

#include <limits.h>
#include <string.h>
#include <stddef.h>

#include "config.h"
#include "gpu_ctx.h"
#include "format.h"
#include "hmap.h"
#include "hwmap.h"
#include "log.h"
#include "memory.h"
#include "internal.h"
#include "pgcraft.h"
#include "precision.h"
#include "type.h"

#if defined(BACKEND_GL) || defined(BACKEND_GLES)
#include "backends/gl/gpu_ctx_gl.h"
#include "backends/gl/feature_gl.h"
#include "backends/gl/program_gl_utils.h"
#endif

enum {
    NGLI_BINDING_TYPE_UBO,
    NGLI_BINDING_TYPE_SSBO,
    NGLI_BINDING_TYPE_TEXTURE,
    NGLI_BINDING_TYPE_NB
};

#define NB_BINDINGS (NGLI_PROGRAM_SHADER_NB * NGLI_BINDING_TYPE_NB)
#define BIND_ID(stage, type) ((stage) * NGLI_BINDING_TYPE_NB + (type))

struct pgcraft_pipeline_info {
    struct {
        struct darray uniforms;   // uniform_desc
        struct darray textures;   // texture_desc
        struct darray buffers;    // buffer_desc
        struct darray attributes; // attribute_desc
    } desc;
    struct {
        struct darray uniforms;   // uniform data pointer
        struct darray textures;   // texture pointer
        struct darray buffers;    // buffer pointer
        struct darray attributes; // attribute pointer
    } data;
};

struct pgcraft {
    struct ngl_ctx *ctx;

    struct darray texture_infos; // pgcraft_texture_info
    struct pgcraft_compat_info compat_info;

    struct bstr *shaders[NGLI_PROGRAM_SHADER_NB];

    struct pgcraft_pipeline_info pipeline_info;
    struct pgcraft_pipeline_info filtered_pipeline_info;

    struct darray vert_out_vars; // pgcraft_iovar

    struct program *program;

    int bindings[NB_BINDINGS];
    int *next_bindings[NB_BINDINGS];
    int next_in_locations[NGLI_PROGRAM_SHADER_NB];
    int next_out_locations[NGLI_PROGRAM_SHADER_NB];

    /* GLSL info */
    int glsl_version;
    const char *glsl_version_suffix;
    const char *sym_vertex_index;
    const char *sym_instance_index;
    const char *rg; // 2-component texture picking (could be either rg or ra depending on the OpenGL version)
    int has_in_out_qualifiers;
    int has_in_out_layout_qualifiers;
    int has_precision_qualifiers;
    int has_modern_texture_picking;
    int has_explicit_bindings;
};

/*
 * Currently unmapped formats: r11f_g11f_b10f, rgb10_a2, rgb10_a2ui
 */
static const struct {
    const char *format;
    const char *prefix;
} image_glsl_format_map[NGLI_FORMAT_NB] = {
    [NGLI_FORMAT_R8_UNORM]             = {"r8",           ""},
    [NGLI_FORMAT_R8_SNORM]             = {"r8_snorm",     ""},
    [NGLI_FORMAT_R8_UINT]              = {"r8ui",         "u"},
    [NGLI_FORMAT_R8_SINT]              = {"r8i",          "i"},
    [NGLI_FORMAT_R8G8_UNORM]           = {"rg8",          ""},
    [NGLI_FORMAT_R8G8_SNORM]           = {"rg8_snorm",    ""},
    [NGLI_FORMAT_R8G8_UINT]            = {"rg8ui",        "u"},
    [NGLI_FORMAT_R8G8_SINT]            = {"rg8i",         "i"},
    [NGLI_FORMAT_R8G8B8_UNORM]         = {NULL,           NULL},
    [NGLI_FORMAT_R8G8B8_SNORM]         = {NULL,           NULL},
    [NGLI_FORMAT_R8G8B8_UINT]          = {NULL,           NULL},
    [NGLI_FORMAT_R8G8B8_SINT]          = {NULL,           NULL},
    [NGLI_FORMAT_R8G8B8_SRGB]          = {NULL,           NULL},
    [NGLI_FORMAT_R8G8B8A8_UNORM]       = {"rgba8",        ""},
    [NGLI_FORMAT_R8G8B8A8_SNORM]       = {"rgba8_snorm",  ""},
    [NGLI_FORMAT_R8G8B8A8_UINT]        = {"rgba8ui",      "u"},
    [NGLI_FORMAT_R8G8B8A8_SINT]        = {"rgba8i",       "i"},
    [NGLI_FORMAT_R8G8B8A8_SRGB]        = {NULL,           NULL},
    [NGLI_FORMAT_B8G8R8A8_UNORM]       = {"rgba8",        ""},
    [NGLI_FORMAT_B8G8R8A8_SNORM]       = {"rgba8_snorm",  ""},
    [NGLI_FORMAT_B8G8R8A8_UINT]        = {"rgba8ui",      "u"},
    [NGLI_FORMAT_B8G8R8A8_SINT]        = {"rgba8i",       "i"},
    [NGLI_FORMAT_R16_UNORM]            = {"r16",          ""},
    [NGLI_FORMAT_R16_SNORM]            = {"r16_snorm",    ""},
    [NGLI_FORMAT_R16_UINT]             = {"r16ui",        "u"},
    [NGLI_FORMAT_R16_SINT]             = {"r16i",         "i"},
    [NGLI_FORMAT_R16_SFLOAT]           = {"r16f",         ""},
    [NGLI_FORMAT_R16G16_UNORM]         = {"rg16",         ""},
    [NGLI_FORMAT_R16G16_SNORM]         = {"rg16_snorm",   ""},
    [NGLI_FORMAT_R16G16_UINT]          = {"rg16ui",       "u"},
    [NGLI_FORMAT_R16G16_SINT]          = {"rg16i",        "i"},
    [NGLI_FORMAT_R16G16_SFLOAT]        = {"rg16f",        ""},
    [NGLI_FORMAT_R16G16B16_UNORM]      = {NULL,           NULL},
    [NGLI_FORMAT_R16G16B16_SNORM]      = {NULL,           NULL},
    [NGLI_FORMAT_R16G16B16_UINT]       = {NULL,           NULL},
    [NGLI_FORMAT_R16G16B16_SINT]       = {NULL,           NULL},
    [NGLI_FORMAT_R16G16B16_SFLOAT]     = {NULL,           NULL},
    [NGLI_FORMAT_R16G16B16A16_UNORM]   = {"rgba16",       ""},
    [NGLI_FORMAT_R16G16B16A16_SNORM]   = {"rgba16_snorm", ""},
    [NGLI_FORMAT_R16G16B16A16_UINT]    = {"rgba16ui",     "u"},
    [NGLI_FORMAT_R16G16B16A16_SINT]    = {"rgba16i",      "i"},
    [NGLI_FORMAT_R16G16B16A16_SFLOAT]  = {"rgba16f",      ""},
    [NGLI_FORMAT_R32_UINT]             = {"r32ui",        "u"},
    [NGLI_FORMAT_R32_SINT]             = {"r32i",         "i"},
    [NGLI_FORMAT_R32_SFLOAT]           = {"r32f",         ""},
    [NGLI_FORMAT_R32G32_UINT]          = {"rg32ui",       "u"},
    [NGLI_FORMAT_R32G32_SINT]          = {"rg32i",        "i"},
    [NGLI_FORMAT_R32G32_SFLOAT]        = {"rg32f",        ""},
    [NGLI_FORMAT_R32G32B32_UINT]       = {NULL,           NULL},
    [NGLI_FORMAT_R32G32B32_SINT]       = {NULL,           NULL},
    [NGLI_FORMAT_R32G32B32_SFLOAT]     = {NULL,           NULL},
    [NGLI_FORMAT_R32G32B32A32_UINT]    = {"rgba32ui",     "u"},
    [NGLI_FORMAT_R32G32B32A32_SINT]    = {"rgba32i",      "i"},
    [NGLI_FORMAT_R32G32B32A32_SFLOAT]  = {"rgba32f",      ""},
    [NGLI_FORMAT_D16_UNORM]            = {NULL,           NULL},
    [NGLI_FORMAT_X8_D24_UNORM_PACK32]  = {NULL,           NULL},
    [NGLI_FORMAT_D32_SFLOAT]           = {NULL,           NULL},
    [NGLI_FORMAT_D24_UNORM_S8_UINT]    = {NULL,           NULL},
    [NGLI_FORMAT_D32_SFLOAT_S8_UINT]   = {NULL,           NULL},
    [NGLI_FORMAT_S8_UINT]              = {NULL,           NULL},
};

enum {
    TYPE_FLAG_IS_SAMPLER_OR_IMAGE = 1 << 0,
    TYPE_FLAG_HAS_PRECISION       = 1 << 1,
    TYPE_FLAG_IS_INT              = 1 << 2,
};

static const int type_flags_map[NGLI_TYPE_NB] = {
    [NGLI_TYPE_I32]                         = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_INT,
    [NGLI_TYPE_IVEC2]                       = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_INT,
    [NGLI_TYPE_IVEC3]                       = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_INT,
    [NGLI_TYPE_IVEC4]                       = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_INT,
    [NGLI_TYPE_U32]                         = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_INT,
    [NGLI_TYPE_UVEC2]                       = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_INT,
    [NGLI_TYPE_UVEC3]                       = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_INT,
    [NGLI_TYPE_UVEC4]                       = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_INT,
    [NGLI_TYPE_F32]                         = TYPE_FLAG_HAS_PRECISION,
    [NGLI_TYPE_VEC2]                        = TYPE_FLAG_HAS_PRECISION,
    [NGLI_TYPE_VEC3]                        = TYPE_FLAG_HAS_PRECISION,
    [NGLI_TYPE_VEC4]                        = TYPE_FLAG_HAS_PRECISION,
    [NGLI_TYPE_MAT3]                        = TYPE_FLAG_HAS_PRECISION,
    [NGLI_TYPE_MAT4]                        = TYPE_FLAG_HAS_PRECISION,
    [NGLI_TYPE_BOOL]                        = 0,
    [NGLI_TYPE_SAMPLER_2D]                  = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_SAMPLER_OR_IMAGE,
    [NGLI_TYPE_SAMPLER_2D_RECT]             = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_SAMPLER_OR_IMAGE,
    [NGLI_TYPE_SAMPLER_3D]                  = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_SAMPLER_OR_IMAGE,
    [NGLI_TYPE_SAMPLER_CUBE]                = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_SAMPLER_OR_IMAGE,
    [NGLI_TYPE_SAMPLER_EXTERNAL_OES]        = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_SAMPLER_OR_IMAGE,
    [NGLI_TYPE_SAMPLER_EXTERNAL_2D_Y2Y_EXT] = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_SAMPLER_OR_IMAGE,
    [NGLI_TYPE_IMAGE_2D]                    = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_SAMPLER_OR_IMAGE,
    [NGLI_TYPE_UNIFORM_BUFFER]              = 0,
    [NGLI_TYPE_STORAGE_BUFFER]              = 0,
};

static int is_sampler_or_image(int type)
{
    return type_flags_map[type] & TYPE_FLAG_IS_SAMPLER_OR_IMAGE;
}

static int type_has_precision(int type)
{
    return type_flags_map[type] & TYPE_FLAG_HAS_PRECISION;
}

static int type_is_int(int type)
{
    return type_flags_map[type] & TYPE_FLAG_IS_INT;
}

static const char *get_glsl_type(int type)
{
    const char *ret = ngli_type_get_name(type);
    ngli_assert(ret);
    return ret;
}

static int request_next_binding(struct pgcraft *s, int stage, int type)
{
    int *next_bind = s->next_bindings[BIND_ID(stage, type)];
    if (!next_bind) {
        /*
         * Non-explicit bindings is still allowed for OpenGL context not
         * supporting explicit locations/bindings.
         */
        return -1;
    }
    return (*next_bind)++;
}

static const char *get_precision_qualifier(const struct pgcraft *s, int type, int precision, const char *defaultp)
{
    if (!s->has_precision_qualifiers || !type_has_precision(type))
        return "";
    static const char *precision_qualifiers[NGLI_PRECISION_NB] = {
        [NGLI_PRECISION_AUTO]   = NULL,
        [NGLI_PRECISION_HIGH]   = "highp",
        [NGLI_PRECISION_MEDIUM] = "mediump",
        [NGLI_PRECISION_LOW]    = "lowp",
    };
    const char *ret = precision_qualifiers[precision];
    return ret ? ret : defaultp;
}

static int inject_block_uniform(struct pgcraft *s, struct bstr *b,
                                const struct pgcraft_uniform *uniform, int stage)
{
    struct pgcraft_compat_info *compat_info = &s->compat_info;
    struct block *block = &compat_info->ublocks[stage];

    return ngli_block_add_field(block, uniform->name, uniform->type, uniform->count);
}

static int inject_uniform(struct pgcraft *s, struct bstr *b,
                          const struct pgcraft_uniform *uniform)
{
    struct pgcraft_compat_info *compat_info = &s->compat_info;
    if (compat_info->use_ublocks)
        return inject_block_uniform(s, b, uniform, uniform->stage);

    struct pipeline_uniform_desc pl_uniform_desc = {
        .type  = uniform->type,
        .count = NGLI_MAX(uniform->count, 1),
    };
    snprintf(pl_uniform_desc.name, sizeof(pl_uniform_desc.name), "%s", uniform->name);

    const char *type = get_glsl_type(uniform->type);
    const char *precision = get_precision_qualifier(s, uniform->type, uniform->precision, "highp");
    if (uniform->count)
        ngli_bstr_printf(b, "uniform %s %s %s[%d];\n", precision, type, uniform->name, uniform->count);
    else
        ngli_bstr_printf(b, "uniform %s %s %s;\n", precision, type, uniform->name);

    if (!ngli_darray_push(&s->pipeline_info.desc.uniforms, &pl_uniform_desc))
        return NGL_ERROR_MEMORY;
    if (!ngli_darray_push(&s->pipeline_info.data.uniforms, &uniform->data))
        return NGL_ERROR_MEMORY;
    return 0;
}

static int inject_uniforms(struct pgcraft *s, struct bstr *b,
                           const struct pgcraft_params *params, int stage)
{
    for (int i = 0; i < params->nb_uniforms; i++) {
        const struct pgcraft_uniform *uniform = &params->uniforms[i];
        if (uniform->stage != stage)
            continue;
        int ret = inject_uniform(s, b, &params->uniforms[i]);
        if (ret < 0)
            return ret;
    }
    return 0;
}

static const char * const texture_info_suffixes[NGLI_INFO_FIELD_NB] = {
    [NGLI_INFO_FIELD_SAMPLING_MODE]     = "_sampling_mode",
    [NGLI_INFO_FIELD_COORDINATE_MATRIX] = "_coord_matrix",
    [NGLI_INFO_FIELD_COLOR_MATRIX]      = "_color_matrix",
    [NGLI_INFO_FIELD_DIMENSIONS]        = "_dimensions",
    [NGLI_INFO_FIELD_TIMESTAMP]         = "_ts",
    [NGLI_INFO_FIELD_SAMPLER_0]         = "",
    [NGLI_INFO_FIELD_SAMPLER_1]         = "_1",
    [NGLI_INFO_FIELD_SAMPLER_2]         = "_2",
    [NGLI_INFO_FIELD_SAMPLER_OES]       = "_oes",
    [NGLI_INFO_FIELD_SAMPLER_RECT_0]    = "_rect_0",
    [NGLI_INFO_FIELD_SAMPLER_RECT_1]    = "_rect_1",
};

static const int texture_types_map[NGLI_PGCRAFT_SHADER_TEX_TYPE_NB][NGLI_INFO_FIELD_NB] = {
    [NGLI_PGCRAFT_SHADER_TEX_TYPE_VIDEO] = {
        [NGLI_INFO_FIELD_COORDINATE_MATRIX] = NGLI_TYPE_MAT4,
        [NGLI_INFO_FIELD_DIMENSIONS]        = NGLI_TYPE_VEC2,
        [NGLI_INFO_FIELD_TIMESTAMP]         = NGLI_TYPE_F32,
        [NGLI_INFO_FIELD_COLOR_MATRIX]      = NGLI_TYPE_MAT4,
        [NGLI_INFO_FIELD_SAMPLING_MODE]     = NGLI_TYPE_I32,
        [NGLI_INFO_FIELD_SAMPLER_0]         = NGLI_TYPE_SAMPLER_2D,
        [NGLI_INFO_FIELD_SAMPLER_1]         = NGLI_TYPE_SAMPLER_2D,
        [NGLI_INFO_FIELD_SAMPLER_2]         = NGLI_TYPE_SAMPLER_2D,
#if defined(TARGET_ANDROID)
        [NGLI_INFO_FIELD_SAMPLER_OES]       = NGLI_TYPE_SAMPLER_EXTERNAL_OES,
#elif defined(TARGET_DARWIN)
        [NGLI_INFO_FIELD_SAMPLER_RECT_0]    = NGLI_TYPE_SAMPLER_2D_RECT,
        [NGLI_INFO_FIELD_SAMPLER_RECT_1]    = NGLI_TYPE_SAMPLER_2D_RECT,
#endif
    },
    [NGLI_PGCRAFT_SHADER_TEX_TYPE_2D] = {
        [NGLI_INFO_FIELD_SAMPLER_0]         = NGLI_TYPE_SAMPLER_2D,
        [NGLI_INFO_FIELD_COORDINATE_MATRIX] = NGLI_TYPE_MAT4,
        [NGLI_INFO_FIELD_DIMENSIONS]        = NGLI_TYPE_VEC2,
        [NGLI_INFO_FIELD_TIMESTAMP]         = NGLI_TYPE_F32,
    },
    [NGLI_PGCRAFT_SHADER_TEX_TYPE_IMAGE_2D] = {
        [NGLI_INFO_FIELD_SAMPLER_0]         = NGLI_TYPE_IMAGE_2D,
        [NGLI_INFO_FIELD_COORDINATE_MATRIX] = NGLI_TYPE_MAT4,
        [NGLI_INFO_FIELD_DIMENSIONS]        = NGLI_TYPE_VEC2,
        [NGLI_INFO_FIELD_TIMESTAMP]         = NGLI_TYPE_F32,
    },
    [NGLI_PGCRAFT_SHADER_TEX_TYPE_3D] = {
        [NGLI_INFO_FIELD_SAMPLER_0]         = NGLI_TYPE_SAMPLER_3D,
        [NGLI_INFO_FIELD_DIMENSIONS]        = NGLI_TYPE_VEC3,
    },
    [NGLI_PGCRAFT_SHADER_TEX_TYPE_CUBE] = {
        [NGLI_INFO_FIELD_SAMPLER_0]         = NGLI_TYPE_SAMPLER_CUBE,
    },
};

static int is_type_supported(struct pgcraft *s, int type)
{
    const struct ngl_ctx *ctx = s->ctx;
    const struct ngl_config *config = &ctx->config;

    switch(type) {
    case NGLI_TYPE_SAMPLER_2D_RECT:
        return ngli_hwmap_is_image_layout_supported(config->backend, NGLI_IMAGE_LAYOUT_RECTANGLE) ||
               ngli_hwmap_is_image_layout_supported(config->backend, NGLI_IMAGE_LAYOUT_NV12_RECTANGLE);
    case NGLI_TYPE_SAMPLER_EXTERNAL_OES:
    case NGLI_TYPE_SAMPLER_EXTERNAL_2D_Y2Y_EXT:
        return ngli_hwmap_is_image_layout_supported(config->backend, NGLI_IMAGE_LAYOUT_MEDIACODEC);
    default:
        return 1;
    }
}

static int prepare_texture_info_fields(struct pgcraft *s, const struct pgcraft_params *params, int graphics,
                                        const struct pgcraft_texture *texture,
                                        struct pgcraft_texture_info *info)
{
    const int *types_map = texture_types_map[texture->type];

    for (int i = 0; i < NGLI_INFO_FIELD_NB; i++) {
        struct pgcraft_texture_info_field *field = &info->fields[i];
        const int type = types_map[i];
        if (type == NGLI_TYPE_NONE || !is_type_supported(s, type))
            continue;
        field->type = type;
        int len = snprintf(field->name, sizeof(field->name), "%s%s", texture->name, texture_info_suffixes[i]);
        if (len >= sizeof(field->name)) {
            LOG(ERROR, "texture name \"%s\" is too long", texture->name);
            return NGL_ERROR_MEMORY;
        }
        if (graphics && i == NGLI_INFO_FIELD_COORDINATE_MATRIX)
            field->stage = NGLI_PROGRAM_SHADER_VERT;
        else
            field->stage = texture->stage;
    }
    return 0;
}

/*
 * A single texture info can be shared between multiple stage, so we need to do
 * a first pass to allocate them with and make them hold all the information
 * needed for the following injection stage.
 * FIXME: this means the internally exposed pgcraft_texture_info contains many
 * unnecessary stuff for our users of the pgcraft API.
 */
static int prepare_texture_infos(struct pgcraft *s, const struct pgcraft_params *params, int graphics)
{
    for (int i = 0; i < params->nb_textures; i++) {
        const struct pgcraft_texture *texture = &params->textures[i];
        ngli_assert(!(texture->type == NGLI_PGCRAFT_SHADER_TEX_TYPE_VIDEO && texture->texture));

        struct pgcraft_texture_info info = {
            .stage     = texture->stage,
            .precision = texture->precision,
            .texture   = texture->texture,
            .image     = texture->image,
            .format    = texture->format,
            .writable  = texture->writable,
        };

        int ret = prepare_texture_info_fields(s, params, graphics, texture, &info);
        if (ret < 0)
            return ret;

        if (!ngli_darray_push(&s->texture_infos, &info))
            return NGL_ERROR_MEMORY;
    }
    return 0;
}

static int inject_texture_info(struct pgcraft *s, struct pgcraft_texture_info *info, int stage)
{
    for (int i = 0; i < NGLI_INFO_FIELD_NB; i++) {
        const struct pgcraft_texture_info_field *field = &info->fields[i];

        if (field->type == NGLI_TYPE_NONE || field->stage != stage)
            continue;

        struct bstr *b = s->shaders[stage];

        if (is_sampler_or_image(field->type)) {
            struct pipeline_texture_desc pl_texture_desc = {
                .type     = field->type,
                .location = -1,
                .binding  = request_next_binding(s, stage, NGLI_BINDING_TYPE_TEXTURE),
                .access   = info->writable ? NGLI_ACCESS_READ_WRITE : NGLI_ACCESS_READ_BIT,
                .stage    = stage,
            };
            snprintf(pl_texture_desc.name, sizeof(pl_texture_desc.name), "%s", field->name);

            const char *prefix = "";
            if (field->type == NGLI_TYPE_IMAGE_2D) {
                if (info->format == NGLI_TYPE_NONE) {
                    LOG(ERROR, "Texture2D.format must be set when accessing it as an image");
                    return NGL_ERROR_INVALID_ARG;
                }
                const char *format = image_glsl_format_map[info->format].format;
                prefix = image_glsl_format_map[info->format].prefix;
                if (!format || !prefix) {
                    LOG(ERROR, "unsupported texture format");
                    return NGL_ERROR_UNSUPPORTED;
                }

                ngli_bstr_printf(b, "layout(%s", format);
                if (pl_texture_desc.binding != -1)
                    ngli_bstr_printf(b, ", binding=%d", pl_texture_desc.binding);

                /*
                 * Restrict memory qualifier according to the OpenGLES 3.2 spec
                 * (Section 4.10. Memory qualifiers):
                 *
                 *     Except for image variables qualified with the format
                 *     qualifiers r32f, r32i, and r32ui, image variables must
                 *     specify a memory qualifier (readonly, writeonly, or both).
                 */
                const char *writable_qualifier= "";
                if (info->format != NGLI_FORMAT_R32_SFLOAT &&
                    info->format != NGLI_FORMAT_R32_SINT &&
                    info->format != NGLI_FORMAT_R32_UINT) {
                    writable_qualifier = "writeonly";
                }
                ngli_bstr_printf(b, ") %s ", info->writable ? writable_qualifier : "readonly");
            } else if (pl_texture_desc.binding != -1) {
                ngli_bstr_printf(b, "layout(binding=%d) ", pl_texture_desc.binding);
            }

            const char *type = get_glsl_type(field->type);
            const char *precision = get_precision_qualifier(s, field->type, info->precision, "lowp");
            ngli_bstr_printf(b, "uniform %s %s%s %s;\n", precision, prefix, type, field->name);

            if (!ngli_darray_push(&s->pipeline_info.desc.textures, &pl_texture_desc))
                return NGL_ERROR_MEMORY;
            if (!ngli_darray_push(&s->pipeline_info.data.textures, &info->texture))
                return NGL_ERROR_MEMORY;
        } else {
            struct pgcraft_uniform uniform = {
                .stage = field->stage,
                .type = field->type,
            };
            snprintf(uniform.name, sizeof(uniform.name), "%s", field->name);
            int ret = inject_uniform(s, b, &uniform);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

static int inject_texture_infos(struct pgcraft *s, const struct pgcraft_params *params, int stage)
{
    struct darray *texture_infos_array = &s->texture_infos;
    struct pgcraft_texture_info *texture_infos = ngli_darray_data(texture_infos_array);
    for (int i = 0; i < ngli_darray_count(texture_infos_array); i++) {
        struct pgcraft_texture_info *info = &texture_infos[i];
        int ret = inject_texture_info(s, info, stage);
        if (ret < 0)
            return ret;
    }
    return 0;
}

static const char *glsl_layout_str_map[NGLI_BLOCK_NB_LAYOUTS] = {
    [NGLI_BLOCK_LAYOUT_STD140] = "std140",
    [NGLI_BLOCK_LAYOUT_STD430] = "std430",
};

static int inject_block(struct pgcraft *s, struct bstr *b,
                        const struct pgcraft_block *named_block)
{
    const struct block *block = named_block->block;
    const int binding_type = named_block->type == NGLI_TYPE_UNIFORM_BUFFER
                           ? NGLI_BINDING_TYPE_UBO : NGLI_BINDING_TYPE_SSBO;
    struct pipeline_buffer_desc pl_buffer_desc = {
        .type    = named_block->type,
        .binding = request_next_binding(s, named_block->stage, binding_type),
        .access  = named_block->writable ? NGLI_ACCESS_READ_WRITE : NGLI_ACCESS_READ_BIT,
        .stage   = named_block->stage,
    };
    int len = snprintf(pl_buffer_desc.name, sizeof(pl_buffer_desc.name), "%s_block", named_block->name);
    if (len >= sizeof(pl_buffer_desc.name)) {
        LOG(ERROR, "block name \"%s\" is too long", named_block->name);
        return NGL_ERROR_MEMORY;
    }

    const char *layout = glsl_layout_str_map[block->layout];
    if (s->has_explicit_bindings) {
        ngli_bstr_printf(b, "layout(%s,binding=%d)", layout, pl_buffer_desc.binding);
    } else {
        ngli_bstr_printf(b, "layout(%s)", layout);
    }

    if (named_block->type == NGLI_TYPE_STORAGE_BUFFER && !named_block->writable)
        ngli_bstr_print(b, " readonly");

    const char *keyword = get_glsl_type(named_block->type);
    ngli_bstr_printf(b, " %s %s_block {\n", keyword, named_block->name);
    const struct block_field *field_info = ngli_darray_data(&block->fields);
    for (int i = 0; i < ngli_darray_count(&block->fields); i++) {
        const struct block_field *fi = &field_info[i];
        const char *type = get_glsl_type(fi->type);
        if (named_block->variadic && fi->count && i == ngli_darray_count(&block->fields))
            ngli_bstr_printf(b, "    %s %s[];\n", type, fi->name);
        else if (fi->count)
            ngli_bstr_printf(b, "    %s %s[%d];\n", type, fi->name, fi->count);
        else
            ngli_bstr_printf(b, "    %s %s;\n", type, fi->name);
    }
    const char *instance_name = named_block->instance_name ? named_block->instance_name : named_block->name;
    ngli_bstr_printf(b, "} %s;\n", instance_name);

    if (!ngli_darray_push(&s->pipeline_info.desc.buffers, &pl_buffer_desc))
        return NGL_ERROR_MEMORY;
    if (!ngli_darray_push(&s->pipeline_info.data.buffers, &named_block->buffer))
        return NGL_ERROR_MEMORY;
    return pl_buffer_desc.binding;
}

static int inject_blocks(struct pgcraft *s, struct bstr *b,
                         const struct pgcraft_params *params, int stage)
{
    for (int i = 0; i < params->nb_blocks; i++) {
        const struct pgcraft_block *block = &params->blocks[i];
        if (block->stage != stage)
            continue;
        int ret = inject_block(s, b, &params->blocks[i]);
        if (ret < 0)
            return ret;
    }
    return 0;
}

static int get_location_count(int type)
{
    switch (type) {
    case NGLI_TYPE_MAT3: return 3;
    case NGLI_TYPE_MAT4: return 4;
    default:             return 1;
    }
}

static int inject_attribute(struct pgcraft *s, struct bstr *b,
                            const struct pgcraft_attribute *attribute)
{
    const char *type = get_glsl_type(attribute->type);
    const int attribute_count = get_location_count(attribute->type);

    const int base_location = s->next_in_locations[NGLI_PROGRAM_SHADER_VERT];
    s->next_in_locations[NGLI_PROGRAM_SHADER_VERT] += attribute_count;

    if (s->has_in_out_layout_qualifiers) {
        ngli_bstr_printf(b, "layout(location=%d) ", base_location);
    }

    const char *qualifier = s->has_in_out_qualifiers ? "in" : "attribute";
    const char *precision = get_precision_qualifier(s, attribute->type, attribute->precision, "highp");
    ngli_bstr_printf(b, "%s %s %s %s;\n", qualifier, precision, type, attribute->name);

    const int attribute_offset = ngli_format_get_bytes_per_pixel(attribute->format);
    for (int i = 0; i < attribute_count; i++) {
        struct pipeline_attribute_desc pl_attribute_desc = {
            .location = base_location + i,
            .format   = attribute->format,
            .stride   = attribute->stride,
            .offset   = attribute->offset + i * attribute_offset,
            .rate     = attribute->rate,
        };
        snprintf(pl_attribute_desc.name, sizeof(pl_attribute_desc.name), "%s", attribute->name);

        if (!ngli_darray_push(&s->pipeline_info.desc.attributes, &pl_attribute_desc))
            return NGL_ERROR_MEMORY;
        if (!ngli_darray_push(&s->pipeline_info.data.attributes, &attribute->buffer))
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

static int inject_attributes(struct pgcraft *s, struct bstr *b,
                             const struct pgcraft_params *params)
{
    for (int i = 0; i < params->nb_attributes; i++) {
        int ret = inject_attribute(s, b, &params->attributes[i]);
        if (ret < 0)
            return ret;
    }
    return 0;
}

const char *ublock_names[] = {
    [NGLI_PROGRAM_SHADER_VERT] = "vert",
    [NGLI_PROGRAM_SHADER_FRAG] = "frag",
    [NGLI_PROGRAM_SHADER_COMP] = "comp",
};

static int inject_ublock(struct pgcraft *s, struct bstr *b, int stage)
{
    struct pgcraft_compat_info *compat_info = &s->compat_info;
    if (!compat_info->use_ublocks)
        return 0;

    struct block *block = &compat_info->ublocks[stage];
    if (!block->size)
        return 0;

    struct pgcraft_block pgcraft_block = {
        /* instance name is empty to make field accesses identical to uniform accesses */
        .instance_name = "",
        .type          = NGLI_TYPE_UNIFORM_BUFFER,
        .stage         = stage,
        .block         = block,
        .buffer        = NULL,
    };
    snprintf(pgcraft_block.name, sizeof(pgcraft_block.name), "ngl_%s", ublock_names[stage]);

    const int binding = inject_block(s, b, &pgcraft_block);
    if (binding < 0)
        return binding;
    compat_info->ubindings[stage] = binding;

    return 0;
}

static int params_have_ssbos(struct pgcraft *s, const struct pgcraft_params *params, int stage)
{
    for (int i = 0; i < params->nb_blocks; i++) {
        const struct pgcraft_block *pgcraft_block = &params->blocks[i];
        if (pgcraft_block->stage == stage && pgcraft_block->type == NGLI_TYPE_STORAGE_BUFFER)
            return 1;
    }
    return 0;
}

static int params_have_images(struct pgcraft *s, const struct pgcraft_params *params, int stage)
{
    const struct darray *texture_infos_array = &s->texture_infos;
    const struct pgcraft_texture_info *texture_infos = ngli_darray_data(texture_infos_array);
    for (int i = 0; i < ngli_darray_count(texture_infos_array); i++) {
        const struct pgcraft_texture_info *info = &texture_infos[i];
        for (int j = 0; j < NGLI_INFO_FIELD_NB; j++) {
            const struct pgcraft_texture_info_field *field = &info->fields[j];
            if (field->stage == stage && field->type == NGLI_TYPE_IMAGE_2D)
                return 1;
        }
    }
    return 0;
}

static void set_glsl_header(struct pgcraft *s, struct bstr *b, const struct pgcraft_params *params, int stage)
{
    struct ngl_ctx *ctx = s->ctx;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    const struct ngl_config *config = &gpu_ctx->config;

    ngli_bstr_printf(b, "#version %d%s\n", s->glsl_version, s->glsl_version_suffix);

    const int require_ssbo_feature = params_have_ssbos(s, params, stage);
    const int require_image_feature = params_have_images(s, params, stage);
#if defined(TARGET_ANDROID)
    const int require_image_external_feature       = ngli_darray_count(&s->texture_infos) > 0 && s->glsl_version  < 300;
    const int require_image_external_essl3_feature = ngli_darray_count(&s->texture_infos) > 0 && s->glsl_version >= 300;
#endif
    const int enable_shader_texture_lod = (gpu_ctx->features & NGLI_FEATURE_SHADER_TEXTURE_LOD) == NGLI_FEATURE_SHADER_TEXTURE_LOD;
    const int enable_texture_3d         = (gpu_ctx->features & NGLI_FEATURE_TEXTURE_3D) == NGLI_FEATURE_TEXTURE_3D;

    const struct {
        int backend;
        const char *extension;
        int glsl_version;
        int required;
    } features[] = {
        /* OpenGL */
        {NGL_BACKEND_OPENGL, "GL_ARB_shading_language_420pack",       420, s->has_explicit_bindings},
        {NGL_BACKEND_OPENGL, "GL_ARB_shader_image_load_store",        420, require_image_feature},
        {NGL_BACKEND_OPENGL, "GL_ARB_shader_image_size",              430, require_image_feature},
        {NGL_BACKEND_OPENGL, "GL_ARB_shader_storage_buffer_object",   430, require_ssbo_feature},
        {NGL_BACKEND_OPENGL, "GL_ARB_compute_shader",                 430, stage == NGLI_PROGRAM_SHADER_COMP},

        /* OpenGLES */
#if defined(TARGET_ANDROID)
        {NGL_BACKEND_OPENGLES, "GL_OES_EGL_image_external",       INT_MAX, require_image_external_feature},
        {NGL_BACKEND_OPENGLES, "GL_OES_EGL_image_external_essl3", INT_MAX, require_image_external_essl3_feature},
#endif
        {NGL_BACKEND_OPENGLES, "GL_EXT_shader_texture_lod",           300, enable_shader_texture_lod},
        {NGL_BACKEND_OPENGLES, "GL_OES_texture_3D",                   300, enable_texture_3d},
        {NGL_BACKEND_OPENGLES, "GL_OES_standard_derivatives",         300, stage == NGLI_PROGRAM_SHADER_FRAG},
    };

    for (int i = 0; i < NGLI_ARRAY_NB(features); i++) {
        if (features[i].backend == config->backend &&
            features[i].glsl_version > s->glsl_version &&
            features[i].required)
            ngli_bstr_printf(b, "#extension %s : require\n", features[i].extension);
    }

    if (ngli_darray_count(&s->texture_infos) > 0) {
        if (s->has_modern_texture_picking)
            ngli_bstr_print(b, "#define ngl_tex2d   texture\n"
                               "#define ngl_tex3d   texture\n"
                               "#define ngl_texcube texture\n");
        else
            ngli_bstr_print(b, "#define ngl_tex2d   texture2D\n"
                               "#define ngl_tex3d   texture3D\n"
                               "#define ngl_texcube textureCube\n");

        if (config->backend == NGL_BACKEND_OPENGLES && s->glsl_version < 300)
            ngli_bstr_print(b, "#define ngl_tex2dlod   texture2DLodEXT\n"
                               "#define ngl_tex3dlod   texture3DLodEXT\n"
                               "#define ngl_texcubelod textureCubeLodEXT\n");
        else
            ngli_bstr_print(b, "#define ngl_tex2dlod   textureLod\n"
                               "#define ngl_tex3dlod   textureLod\n"
                               "#define ngl_texcubelod textureLod\n");
    }

    ngli_bstr_print(b, "\n");
}

static int texture_needs_clamping(const struct pgcraft_params *params,
                                  const char *name, int name_len)
{
    for (int i = 0; i < params->nb_textures; i++) {
        const struct pgcraft_texture *pgcraft_texture = &params->textures[i];
        if (!strncmp(name, pgcraft_texture->name, name_len))
            return pgcraft_texture->clamp_video;
    }
    return 0;
}

static enum pgcraft_shader_tex_type get_texture_type(const struct pgcraft_params *params,
                                                     const char *name, int name_len)
{
    for (int i = 0; i < params->nb_textures; i++) {
        const struct pgcraft_texture *pgcraft_texture = &params->textures[i];
        if (!strncmp(name, pgcraft_texture->name, name_len))
            return pgcraft_texture->type;
    }
    return NGLI_PGCRAFT_SHADER_TEX_TYPE_NONE;
}

#define WHITESPACES     "\r\n\t "
#define TOKEN_ID_CHARS  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"

static const char *read_token_id(const char *p, char *buf, size_t size)
{
    const size_t len = strspn(p, TOKEN_ID_CHARS);
    snprintf(buf, size, "%.*s", (int)len, p);
    return p + len;
}

static const char *skip_arg(const char *p)
{
    /*
     * TODO: need to error out on directive lines since evaluating them is too
     * complex (and you could close a '(' in a #ifdef and close again in the
     * #else branch, so it's a problem for us)
     */
    int opened_paren = 0;
    while (*p) {
        if (*p == ',' && !opened_paren) {
            break;
        } else if (*p == '(') {
            opened_paren++;
            p++;
        } else if (*p == ')') {
            if (opened_paren == 0)
                break;
            opened_paren--;
            p++;
        } else if (!strncmp(p, "//", 2)) {
            p += strcspn(p, "\r\n");
            // TODO: skip to EOL (handle '\' at EOL?)
        } else if (!strncmp(p, "/*", 2)) {
            p += 2;
            const char *eoc = strstr(p, "*/");
            if (eoc)
                p = eoc + 2;
        } else {
            p++;
        }
    }
    return p;
}

struct token {
    char id[16];
    ptrdiff_t pos;
};

#define ARG_FMT(x) (int)x##_len, x##_start

static int handle_token(struct pgcraft *s, const struct pgcraft_params *params,
                        const struct token *token, const char *p, struct bstr *dst)
{
    struct ngl_ctx *ctx = s->ctx;
    const struct ngl_config *config = &ctx->config;

    /* Skip "ngl_XXX(" and the whitespaces */
    p += strlen(token->id);
    p += strspn(p, WHITESPACES);
    if (*p++ != '(')
        return NGL_ERROR_INVALID_ARG;
    p += strspn(p, WHITESPACES);

    /* Extract the first argument (texture base name) from which we later
     * derive all the uniform names */
    const char *arg0_start = p;
    p = skip_arg(p);
    ptrdiff_t arg0_len = p - arg0_start;

    if (!strcmp(token->id, "ngl_texvideo")) {
        if (*p != ',')
            return NGL_ERROR_INVALID_ARG;
        p++;
        p += strspn(p, WHITESPACES);

        const char *coords_start = p;
        p = skip_arg(p);
        ptrdiff_t coords_len = p - coords_start;
        if (*p != ')')
            return NGL_ERROR_INVALID_ARG;
        p++;

        const enum pgcraft_shader_tex_type texture_type = get_texture_type(params, arg0_start, arg0_len);
        if (texture_type != NGLI_PGCRAFT_SHADER_TEX_TYPE_VIDEO) {
            ngli_bstr_printf(dst, "ngl_tex2d(%.*s, %.*s)", ARG_FMT(arg0), ARG_FMT(coords));
            ngli_bstr_print(dst, p);
            return 0;
        }

        const int clamp = texture_needs_clamping(params, arg0_start, arg0_len);
        if (clamp)
            ngli_bstr_print(dst, "clamp(");

        ngli_bstr_print(dst, "(");

        if (ngli_hwmap_is_image_layout_supported(config->backend, NGLI_IMAGE_LAYOUT_MEDIACODEC)) {
            ngli_bstr_printf(dst, "%.*s_sampling_mode == %d ? ", ARG_FMT(arg0), NGLI_IMAGE_LAYOUT_MEDIACODEC);
            ngli_bstr_printf(dst, "ngl_tex2d(%.*s_oes, %.*s) : ", ARG_FMT(arg0), ARG_FMT(coords));
        }

        if (ngli_hwmap_is_image_layout_supported(config->backend, NGLI_IMAGE_LAYOUT_NV12_RECTANGLE)) {
            ngli_bstr_printf(dst, " %.*s_sampling_mode == %d ? ", ARG_FMT(arg0), NGLI_IMAGE_LAYOUT_NV12_RECTANGLE);
            ngli_bstr_printf(dst, "%.*s_color_matrix * vec4(ngl_tex2d(%.*s_rect_0, (%.*s) * %.*s_dimensions).r, "
                                                           "ngl_tex2d(%.*s_rect_1, (%.*s) * %.*s_dimensions / 2.0).rg, 1.0) : ",
                             ARG_FMT(arg0),
                             ARG_FMT(arg0), ARG_FMT(coords), ARG_FMT(arg0),
                             ARG_FMT(arg0), ARG_FMT(coords), ARG_FMT(arg0));
        }

        if (ngli_hwmap_is_image_layout_supported(config->backend, NGLI_IMAGE_LAYOUT_RECTANGLE)) {
            ngli_bstr_printf(dst, "%.*s_sampling_mode == %d ? ", ARG_FMT(arg0), NGLI_IMAGE_LAYOUT_RECTANGLE);
            ngli_bstr_printf(dst, "ngl_tex2d(%.*s_rect_0, (%.*s) * %.*s_dimensions) : ",
                             ARG_FMT(arg0), ARG_FMT(coords), ARG_FMT(arg0));
        }

        if (ngli_hwmap_is_image_layout_supported(config->backend, NGLI_IMAGE_LAYOUT_NV12)) {
            ngli_bstr_printf(dst, "%.*s_sampling_mode == %d ? ", ARG_FMT(arg0), NGLI_IMAGE_LAYOUT_NV12);
            ngli_bstr_printf(dst, "%.*s_color_matrix * vec4(ngl_tex2d(%.*s,   %.*s).r, "
                                                           "ngl_tex2d(%.*s_1, %.*s).%s, 1.0) : ",
                             ARG_FMT(arg0),
                             ARG_FMT(arg0), ARG_FMT(coords),
                             ARG_FMT(arg0), ARG_FMT(coords), s->rg);
        }

        if (ngli_hwmap_is_image_layout_supported(config->backend, NGLI_IMAGE_LAYOUT_YUV)) {
            ngli_bstr_printf(dst, "%.*s_sampling_mode == %d ? ", ARG_FMT(arg0), NGLI_IMAGE_LAYOUT_YUV);
            ngli_bstr_printf(dst, "%.*s_color_matrix * vec4(ngl_tex2d(%.*s,   %.*s).r, "
                                                           "ngl_tex2d(%.*s_1, %.*s).r, "
                                                           "ngl_tex2d(%.*s_2, %.*s).r, 1.0) : ",
                             ARG_FMT(arg0),
                             ARG_FMT(arg0), ARG_FMT(coords),
                             ARG_FMT(arg0), ARG_FMT(coords),
                             ARG_FMT(arg0), ARG_FMT(coords));
        }

        if (ngli_hwmap_is_image_layout_supported(config->backend, NGLI_IMAGE_LAYOUT_DEFAULT)) {
            ngli_bstr_printf(dst, "ngl_tex2d(%.*s, %.*s)", ARG_FMT(arg0), ARG_FMT(coords));
        } else {
            LOG(WARNING, "default image layout not supported in current build");
            ngli_bstr_print(dst, "vec4(1.0, 0.0, 0.0, 1.0)"); /* red color */
        }

        ngli_bstr_print(dst, ")");
        if (clamp)
            ngli_bstr_print(dst, ", 0.0, 1.0)");
        ngli_bstr_print(dst, p);
    } else {
        ngli_assert(0);
    }
    return 0;
}

/*
 * We can not make use of the GLSL preproc to create these custom ngl_*()
 * operators because token pasting (##) is needed but illegal in GLES.
 *
 * Implementing a complete preprocessor is too much of a hassle and risky,
 * especially since we need to evaluate all directives in addition to ours.
 * Instead, we do a simple search & replace for our custom texture helpers. We
 * make sure it supports basic nesting, but aside from that, it's pretty basic.
 */
static int samplers_preproc(struct pgcraft *s, const struct pgcraft_params *params, struct bstr *b)
{
    /*
     * If there is no texture, no point in looking for these custom "ngl_"
     * texture picking symbols.
     */
    if (!ngli_darray_data(&s->texture_infos))
        return 0;

    struct bstr *tmp_buf = ngli_bstr_create();
    if (!tmp_buf)
        return NGL_ERROR_MEMORY;

    /*
     * Construct a stack of "ngl*" tokens found in the shader.
     */
    struct darray token_stack;
    ngli_darray_init(&token_stack, sizeof(struct token), 0);
    const char *base_str = ngli_bstr_strptr(b);
    const char *p = base_str;
    while ((p = strstr(p, "ngl"))) {
        struct token token = {.pos = p - base_str};
        p = read_token_id(p, token.id, sizeof(token.id));
        if (strcmp(token.id, "ngl_texvideo"))
            continue;
        ngli_darray_push(&token_stack, &token);
    }

    /*
     * Read and process the stack from the bottom-up so that we know there is
     * never anything left to substitute up until the end of the buffer.
     */
    int ret = 0;
    const struct token *tokens = ngli_darray_data(&token_stack);
    const int nb_tokens = ngli_darray_count(&token_stack);
    for (int i = 0; i < nb_tokens; i++) {
        const struct token *token = &tokens[nb_tokens - i - 1];
        ngli_bstr_clear(tmp_buf);

        /*
         * We get back the pointer in case it changed in a previous iteration
         * (internal realloc while extending it). The token offset on the other
         * hand wouldn't change since we're doing the replacements backward.
         */
        p = ngli_bstr_strptr(b);
        ret = handle_token(s, params, token, p + token->pos, tmp_buf);
        if (ret < 0)
            break;

        /*
         * The token function did print into the temporary buffer everything
         * up until the end of the buffer, so we can just truncate the main
         * buffer, and re-append the new payload.
         */
        ngli_bstr_truncate(b, token->pos);
        ngli_bstr_print(b, ngli_bstr_strptr(tmp_buf));
    }

    ngli_darray_reset(&token_stack);
    ngli_bstr_freep(&tmp_buf);
    return ret;
}

static int inject_iovars(struct pgcraft *s, struct bstr *b, int stage)
{
    static const char *qualifiers[2][2] = {
        [NGLI_PROGRAM_SHADER_VERT] = {"varying", "out"},
        [NGLI_PROGRAM_SHADER_FRAG] = {"varying", "in"},
    };
    const char *qualifier = qualifiers[stage][s->has_in_out_qualifiers];
    const struct pgcraft_iovar *iovars = ngli_darray_data(&s->vert_out_vars);
    int location = 0;
    for (int i = 0; i < ngli_darray_count(&s->vert_out_vars); i++) {
        if (s->has_in_out_layout_qualifiers)
            ngli_bstr_printf(b, "layout(location=%d) ", location);
        const struct pgcraft_iovar *iovar = &iovars[i];
        const char *precision = stage == NGLI_PROGRAM_SHADER_VERT
                              ? get_precision_qualifier(s, iovar->type, iovar->precision_out, "highp")
                              : get_precision_qualifier(s, iovar->type, iovar->precision_in, "highp");
        const char *type = get_glsl_type(iovar->type);
        if (type_is_int(iovar->type))
            ngli_bstr_print(b, "flat ");
        ngli_bstr_printf(b, "%s %s %s %s;\n", qualifier, precision, type, iovar->name);
        location += get_location_count(iovar->type);
    }
    return 0;
}

static int craft_vert(struct pgcraft *s, const struct pgcraft_params *params)
{
    struct bstr *b = s->shaders[NGLI_PROGRAM_SHADER_VERT];

    set_glsl_header(s, b, params, NGLI_PROGRAM_SHADER_VERT);

    ngli_bstr_printf(b, "#define ngl_out_pos gl_Position\n"
                        "#define ngl_vertex_index %s\n"
                        "#define ngl_instance_index %s\n",
                        s->sym_vertex_index, s->sym_instance_index);

    int ret;
    if ((ret = inject_iovars(s, b, NGLI_PROGRAM_SHADER_VERT)) < 0 ||
        (ret = inject_uniforms(s, b, params, NGLI_PROGRAM_SHADER_VERT)) < 0 ||
        (ret = inject_texture_infos(s, params, NGLI_PROGRAM_SHADER_VERT)) < 0 ||
        (ret = inject_blocks(s, b, params, NGLI_PROGRAM_SHADER_VERT)) < 0 ||
        (ret = inject_attributes(s, b, params)) < 0 ||
        (ret = inject_ublock(s, b, NGLI_PROGRAM_SHADER_VERT)) < 0)
        return ret;

    ngli_bstr_print(b, params->vert_base);
    return samplers_preproc(s, params, b);
}

static int craft_frag(struct pgcraft *s, const struct pgcraft_params *params)

{
    struct bstr *b = s->shaders[NGLI_PROGRAM_SHADER_FRAG];

    set_glsl_header(s, b, params, NGLI_PROGRAM_SHADER_FRAG);

    if (s->has_precision_qualifiers)
        ngli_bstr_print(b, "#if GL_FRAGMENT_PRECISION_HIGH\n"
                           "precision highp float;\n"
                           "#else\n"
                           "precision mediump float;\n"
                           "#endif\n");
    else
        /*
         * The OpenGL wiki states the following:
         *     Precision qualifiers in GLSL are supported for compatibility
         *     with OpenGL ES. They use the same syntax as ES's qualifiers, but
         *     they have no functional effects.
         *  But as safety measure, we define them anyway.
         */
        ngli_bstr_print(b, "#define lowp\n"
                           "#define mediump\n"
                           "#define highp\n");

    ngli_bstr_print(b, "\n");

    if (s->has_in_out_qualifiers) {
        if (s->has_in_out_layout_qualifiers) {
            const int out_location = s->next_out_locations[NGLI_PROGRAM_SHADER_FRAG]++;
            ngli_bstr_printf(b, "layout(location=%d) ", out_location);
        }
        if (params->nb_frag_output)
            ngli_bstr_printf(b, "out vec4 ngl_out_color[%d];\n", params->nb_frag_output);
        else
            ngli_bstr_print(b, "out vec4 ngl_out_color;\n");
    } else {
        ngli_bstr_print(b, "#define ngl_out_color gl_FragColor\n");
    }

    int ret;
    if ((ret = inject_iovars(s, b, NGLI_PROGRAM_SHADER_FRAG)) < 0 ||
        (ret = inject_uniforms(s, b, params, NGLI_PROGRAM_SHADER_FRAG)) < 0 ||
        (ret = inject_texture_infos(s, params, NGLI_PROGRAM_SHADER_FRAG)) < 0 ||
        (ret = inject_blocks(s, b, params, NGLI_PROGRAM_SHADER_FRAG)) < 0 ||
        (ret = inject_ublock(s, b, NGLI_PROGRAM_SHADER_FRAG)) < 0)
        return ret;

    ngli_bstr_print(b, "\n");

    ngli_bstr_print(b, params->frag_base);
    return samplers_preproc(s, params, b);
}

static int craft_comp(struct pgcraft *s, const struct pgcraft_params *params)
{
    struct bstr *b = s->shaders[NGLI_PROGRAM_SHADER_COMP];

    set_glsl_header(s, b, params, NGLI_PROGRAM_SHADER_COMP);

    const int *wg_size = params->workgroup_size;
    ngli_assert(wg_size[0] >= 0 && wg_size[1] >= 0 && wg_size[2] >= 0);
    ngli_bstr_printf(b, "layout(local_size_x=%d, local_size_y=%d, local_size_z=%d) in;\n", NGLI_ARG_VEC3(wg_size));

    int ret;
    if ((ret = inject_uniforms(s, b, params, NGLI_PROGRAM_SHADER_COMP)) < 0 ||
        (ret = inject_texture_infos(s, params, NGLI_PROGRAM_SHADER_COMP)) < 0 ||
        (ret = inject_blocks(s, b, params, NGLI_PROGRAM_SHADER_COMP)) < 0 ||
        (ret = inject_ublock(s, b, NGLI_PROGRAM_SHADER_COMP)) < 0)
        return ret;

    ngli_bstr_print(b, params->comp_base);
    return samplers_preproc(s, params, b);
}

static int probe_pipeline_uniform(const struct hmap *info_map, void *arg)
{
    struct pipeline_uniform_desc *elem_desc = arg;
    /* Remove uniform from the filtered list if it has been stripped during
     * shader compilation */
    const struct program_variable_info *info = ngli_hmap_get(info_map, elem_desc->name);
    if (!info)
        return NGL_ERROR_NOT_FOUND;
    return 0;
}

static int probe_pipeline_buffer(const struct hmap *info_map, void *arg)
{
    if (!info_map)
        return 0;
    struct pipeline_buffer_desc *elem_desc = arg;
    /* Remove buffer from the filtered list if it has been stripped during
     * shader compilation */
    const struct program_variable_info *info = ngli_hmap_get(info_map, elem_desc->name);
    if (!info)
        return NGL_ERROR_NOT_FOUND;
    return 0;
}

static int probe_pipeline_texture(const struct hmap *info_map, void *arg)
{
    struct pipeline_texture_desc *elem_desc = arg;
    if (elem_desc->location != -1)
        return 0;
    const struct program_variable_info *info = ngli_hmap_get(info_map, elem_desc->name);
    if (!info)
        return NGL_ERROR_NOT_FOUND;
    elem_desc->location = info->location;
    if (elem_desc->binding == -1)
        elem_desc->binding = info->binding;
    return elem_desc->location != -1 ? 0 : NGL_ERROR_NOT_FOUND;
}

static int probe_pipeline_attribute(const struct hmap *info_map, void *arg)
{
    if (!info_map)
        return 0;
    struct pipeline_attribute_desc *elem_desc = arg;
    /* Remove attribute from the filtered list if it has been stripped during
     * shader compilation */
    const struct program_variable_info *info = ngli_hmap_get(info_map, elem_desc->name);
    if (!info)
        return NGL_ERROR_NOT_FOUND;
    return 0;
}

typedef int (*probe_func_type)(const struct hmap *info_map, void *arg);

static int filter_pipeline_elems(struct pgcraft *s, probe_func_type probe_func,
                                 const struct hmap *info_map,
                                 struct darray *src_desc, struct darray *src_data,
                                 struct darray *dst_desc, struct darray *dst_data)
{
    uint8_t *desc_elems = ngli_darray_data(src_desc);
    uint8_t *data_elems = ngli_darray_data(src_data);
    int num_elems = ngli_darray_count(src_desc);
    for (int i = 0; i < num_elems; i++) {
        void *desc_elem = desc_elems + i * src_desc->element_size;
        void *data_elem = data_elems + i * src_data->element_size;
        if (info_map && probe_func(info_map, desc_elem) < 0)
            continue;
        if (!ngli_darray_push(dst_desc, desc_elem))
            return NGL_ERROR_MEMORY;
        if (!ngli_darray_push(dst_data, data_elem))
            return NGL_ERROR_MEMORY;
    }
    ngli_darray_reset(src_desc);
    ngli_darray_reset(src_data);
    return 0;
}

static int get_uniform_index(const struct pgcraft *s, const char *name)
{
    const struct pipeline_uniform_desc *pipeline_uniform_descs = ngli_darray_data(&s->filtered_pipeline_info.desc.uniforms);
    for (int i = 0; i < ngli_darray_count(&s->filtered_pipeline_info.desc.uniforms); i++) {
        const struct pipeline_uniform_desc *pipeline_uniform_desc = &pipeline_uniform_descs[i];
        if (!strcmp(pipeline_uniform_desc->name, name))
            return i;
    }
    return -1;
}

static int get_ublock_index(const struct pgcraft *s, const char *name, int stage)
{
    const struct pgcraft_compat_info *compat_info = &s->compat_info;
    const struct darray *fields_array = &compat_info->ublocks[stage].fields;
    const struct block_field *fields = ngli_darray_data(fields_array);
    for (int i = 0; i < ngli_darray_count(fields_array); i++)
        if (!strcmp(fields[i].name, name))
            return stage << 16 | i;
    return -1;
}

static int get_texture_index(const struct pgcraft *s, const char *name)
{
    const struct pipeline_texture_desc *pipeline_texture_descs = ngli_darray_data(&s->filtered_pipeline_info.desc.textures);
    for (int i = 0; i < ngli_darray_count(&s->filtered_pipeline_info.desc.textures); i++) {
        const struct pipeline_texture_desc *pipeline_texture_desc = &pipeline_texture_descs[i];
        if (!strcmp(pipeline_texture_desc->name, name))
            return i;
    }
    return -1;
}

static void probe_texture_info_elems(const struct pgcraft *s, struct pgcraft_texture_info_field *fields)
{
    for (int i = 0; i < NGLI_INFO_FIELD_NB; i++) {
        struct pgcraft_texture_info_field *field = &fields[i];
        if (field->type == NGLI_TYPE_NONE)
            field->index = -1;
        else if (is_sampler_or_image(field->type))
            field->index = get_texture_index(s, field->name);
        else
            field->index = ngli_pgcraft_get_uniform_index(s, field->name, field->stage);
    }
}

static void probe_texture_infos(struct pgcraft *s)
{
    struct darray *texture_infos_array = &s->texture_infos;
    struct pgcraft_texture_info *texture_infos = ngli_darray_data(texture_infos_array);
    for (int i = 0; i < ngli_darray_count(texture_infos_array); i++) {
        struct pgcraft_texture_info *info = &texture_infos[i];
        probe_texture_info_elems(s, info->fields);
    }
}

/*
 * Fill location/binding of pipeline params if they are not set by probing the
 * shader. Also fill the filtered array with available entries.
 */
static int probe_pipeline_elems(struct pgcraft *s)
{
    int ret;

    const struct hmap *uniforms_info   = s->program->uniforms;
    const struct hmap *buffers_info    = s->program->buffer_blocks;
    const struct hmap *attributes_info = s->program->attributes;

    struct pgcraft_pipeline_info *info  = &s->pipeline_info;
    struct pgcraft_pipeline_info *finfo = &s->filtered_pipeline_info;
    if ((ret = filter_pipeline_elems(s, probe_pipeline_uniform,   uniforms_info,   &info->desc.uniforms,   &info->data.uniforms,   &finfo->desc.uniforms,   &finfo->data.uniforms))   < 0 ||
        (ret = filter_pipeline_elems(s, probe_pipeline_buffer,    buffers_info,    &info->desc.buffers,    &info->data.buffers,    &finfo->desc.buffers,    &finfo->data.buffers))    < 0 ||
        (ret = filter_pipeline_elems(s, probe_pipeline_texture,   uniforms_info,   &info->desc.textures,   &info->data.textures,   &finfo->desc.textures,   &finfo->data.textures))   < 0 ||
        (ret = filter_pipeline_elems(s, probe_pipeline_attribute, attributes_info, &info->desc.attributes, &info->data.attributes, &finfo->desc.attributes, &finfo->data.attributes)) < 0)
        return ret;

    probe_texture_infos(s);
    return 0;
}

#if defined(BACKEND_GL) || defined(BACKEND_GLES)

#define IS_GLSL_ES_MIN(min) (config->backend == NGL_BACKEND_OPENGLES && s->glsl_version >= (min))
#define IS_GLSL_MIN(min)    (config->backend == NGL_BACKEND_OPENGL   && s->glsl_version >= (min))

static void setup_glsl_info_gl(struct pgcraft *s)
{
    struct ngl_ctx *ctx = s->ctx;
    const struct ngl_config *config = &ctx->config;
    struct gpu_ctx *gpu_ctx = ctx->gpu_ctx;
    const struct gpu_ctx_gl *gpu_ctx_gl = (struct gpu_ctx_gl *)gpu_ctx;
    const struct glcontext *gl = gpu_ctx_gl->glcontext;

    s->sym_vertex_index   = "gl_VertexID";
    s->sym_instance_index = "gl_InstanceID";

    s->glsl_version = gpu_ctx->language_version;

    if (config->backend == NGL_BACKEND_OPENGLES) {
        if (gpu_ctx->version >= 300) {
            s->glsl_version_suffix = " es";
        } else {
            s->rg = "ra";
        }
    }

    s->has_in_out_qualifiers        = IS_GLSL_ES_MIN(300) || IS_GLSL_MIN(150);
    s->has_in_out_layout_qualifiers = IS_GLSL_ES_MIN(310) || IS_GLSL_MIN(410);
    s->has_precision_qualifiers     = IS_GLSL_ES_MIN(100);
    s->has_modern_texture_picking   = IS_GLSL_ES_MIN(300) || IS_GLSL_MIN(330);
    s->compat_info.use_ublocks       = 0;

    s->has_explicit_bindings = IS_GLSL_ES_MIN(310) || IS_GLSL_MIN(420) ||
                               (gl->features & NGLI_FEATURE_GL_SHADING_LANGUAGE_420PACK);

    /*
     * Bindings are shared across all stages. UBO, SSBO and texture bindings
     * use distinct binding points.
     */
    for (int i = 0; i < NGLI_PROGRAM_SHADER_NB; i++) {
        s->next_bindings[BIND_ID(i, NGLI_BINDING_TYPE_UBO)] = &s->bindings[0];
        s->next_bindings[BIND_ID(i, NGLI_BINDING_TYPE_SSBO)] = &s->bindings[1];
        s->next_bindings[BIND_ID(i, NGLI_BINDING_TYPE_TEXTURE)] = &s->bindings[2];
    }

    /* Force non-explicit texture bindings for contexts that do not support
     * explicit locations and bindings */
    if (!s->has_explicit_bindings) {
        s->next_bindings[BIND_ID(NGLI_PROGRAM_SHADER_VERT, NGLI_BINDING_TYPE_TEXTURE)] = NULL;
        s->next_bindings[BIND_ID(NGLI_PROGRAM_SHADER_FRAG, NGLI_BINDING_TYPE_TEXTURE)] = NULL;
        s->next_bindings[BIND_ID(NGLI_PROGRAM_SHADER_COMP, NGLI_BINDING_TYPE_TEXTURE)] = NULL;
    }
}
#endif

#if defined(BACKEND_VK)
static void setup_glsl_info_vk(struct pgcraft *s)
{
    s->glsl_version = 450;

    s->sym_vertex_index   = "gl_VertexIndex";
    s->sym_instance_index = "gl_InstanceIndex";

    s->has_explicit_bindings        = 1;
    s->has_in_out_qualifiers        = 1;
    s->has_in_out_layout_qualifiers = 1;
    s->has_precision_qualifiers     = 0;
    s->has_modern_texture_picking   = 1;
    s->compat_info.use_ublocks      = 1;

    /* Bindings are shared across stages and types */
    for (int i = 0; i < NB_BINDINGS; i++)
        s->next_bindings[i] = &s->bindings[0];
}
#endif

static void setup_glsl_info(struct pgcraft *s)
{
    struct ngl_ctx *ctx = s->ctx;
    ngli_unused const struct ngl_config *config = &ctx->config;

    s->rg = "rg";
    s->glsl_version_suffix = "";

#if defined(BACKEND_GL) || defined(BACKEND_GLES)
    if (config->backend == NGL_BACKEND_OPENGL || config->backend == NGL_BACKEND_OPENGLES) {
        setup_glsl_info_gl(s);
        return;
    }
#endif

#if defined(BACKEND_VK)
    if (config->backend == NGL_BACKEND_VULKAN) {
        setup_glsl_info_vk(s);
        return;
    }
#endif

    ngli_assert(0);
}

struct pgcraft *ngli_pgcraft_create(struct ngl_ctx *ctx)
{
    struct pgcraft *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;

    s->ctx = ctx;

    setup_glsl_info(s);

    ngli_darray_init(&s->texture_infos, sizeof(struct pgcraft_texture_info), 0);

    struct pgcraft_compat_info *compat_info = &s->compat_info;
    if (compat_info->use_ublocks) {
        for (int i = 0; i < NGLI_ARRAY_NB(compat_info->ublocks); i++) {
            ngli_block_init(&compat_info->ublocks[i], NGLI_BLOCK_LAYOUT_STD140);
            compat_info->ubindings[i] = -1;
        }
    }

    ngli_darray_init(&s->pipeline_info.desc.uniforms,   sizeof(struct pipeline_uniform_desc),   0);
    ngli_darray_init(&s->pipeline_info.desc.textures,   sizeof(struct pipeline_texture_desc),   0);
    ngli_darray_init(&s->pipeline_info.desc.buffers,    sizeof(struct pipeline_buffer_desc),    0);
    ngli_darray_init(&s->pipeline_info.desc.attributes, sizeof(struct pipeline_attribute_desc), 0);

    ngli_darray_init(&s->filtered_pipeline_info.desc.uniforms,   sizeof(struct pipeline_uniform_desc),   0);
    ngli_darray_init(&s->filtered_pipeline_info.desc.textures,   sizeof(struct pipeline_texture_desc),   0);
    ngli_darray_init(&s->filtered_pipeline_info.desc.buffers,    sizeof(struct pipeline_buffer_desc),    0);
    ngli_darray_init(&s->filtered_pipeline_info.desc.attributes, sizeof(struct pipeline_attribute_desc), 0);

    ngli_darray_init(&s->pipeline_info.data.uniforms,   sizeof(void *),           0);
    ngli_darray_init(&s->pipeline_info.data.textures,   sizeof(struct texture *), 0);
    ngli_darray_init(&s->pipeline_info.data.buffers,    sizeof(struct buffer *),  0);
    ngli_darray_init(&s->pipeline_info.data.attributes, sizeof(struct buffer *),  0);

    ngli_darray_init(&s->filtered_pipeline_info.data.uniforms,   sizeof(void *),           0);
    ngli_darray_init(&s->filtered_pipeline_info.data.textures,   sizeof(struct texture *), 0);
    ngli_darray_init(&s->filtered_pipeline_info.data.buffers,    sizeof(struct buffer *),  0);
    ngli_darray_init(&s->filtered_pipeline_info.data.attributes, sizeof(struct buffer *),  0);

    return s;
}

static int alloc_shader(struct pgcraft *s, int stage)
{
    ngli_assert(!s->shaders[stage]);
    struct bstr *b = ngli_bstr_create();
    if (!b)
        return NGL_ERROR_MEMORY;
    s->shaders[stage] = b;
    return 0;
}

static int get_program_compute(struct pgcraft *s, const struct pgcraft_params *params)
{
    int ret;

    if ((ret = alloc_shader(s, NGLI_PROGRAM_SHADER_COMP)) < 0 ||
        (ret = prepare_texture_infos(s, params, 0)) < 0 ||
        (ret = craft_comp(s, params)) < 0)
        return ret;

    const struct program_params program_params = {
        .label   = params->program_label,
        .compute = ngli_bstr_strptr(s->shaders[NGLI_PROGRAM_SHADER_COMP]),
    };
    ret = ngli_pgcache_get_compute_program(&s->ctx->pgcache, &s->program, &program_params);
    ngli_bstr_freep(&s->shaders[NGLI_PROGRAM_SHADER_COMP]);
    return ret;
}

static int get_program_graphics(struct pgcraft *s, const struct pgcraft_params *params)
{
    int ret;

    ngli_darray_init(&s->vert_out_vars, sizeof(struct pgcraft_iovar), 0);
    for (int i = 0; i < params->nb_vert_out_vars; i++) {
        struct pgcraft_iovar *iovar = ngli_darray_push(&s->vert_out_vars, &params->vert_out_vars[i]);
        if (!iovar)
            return NGL_ERROR_MEMORY;
    }

    if ((ret = alloc_shader(s, NGLI_PROGRAM_SHADER_VERT)) < 0 ||
        (ret = alloc_shader(s, NGLI_PROGRAM_SHADER_FRAG)) < 0 ||
        (ret = prepare_texture_infos(s, params, 1)) < 0 ||
        (ret = craft_vert(s, params)) < 0 ||
        (ret = craft_frag(s, params)) < 0)
        return ret;

    const struct program_params program_params = {
        .label    = params->program_label,
        .vertex   = ngli_bstr_strptr(s->shaders[NGLI_PROGRAM_SHADER_VERT]),
        .fragment = ngli_bstr_strptr(s->shaders[NGLI_PROGRAM_SHADER_FRAG]),
    };
    ret = ngli_pgcache_get_graphics_program(&s->ctx->pgcache, &s->program, &program_params);
    ngli_bstr_freep(&s->shaders[NGLI_PROGRAM_SHADER_VERT]);
    ngli_bstr_freep(&s->shaders[NGLI_PROGRAM_SHADER_FRAG]);
    return ret;
}

int ngli_pgcraft_craft(struct pgcraft *s, const struct pgcraft_params *params)
{
    int ret = params->comp_base ? get_program_compute(s, params)
                                : get_program_graphics(s, params);
    if (ret < 0)
        return ret;

    ret = probe_pipeline_elems(s);
    if (ret < 0)
        return ret;

#if defined(BACKEND_GL) || defined(BACKEND_GLES)
    struct ngl_ctx *ctx = s->ctx;
    struct ngl_config *config = &ctx->config;
    if (config->backend == NGL_BACKEND_OPENGL ||
        config->backend == NGL_BACKEND_OPENGLES) {
        if (!s->has_explicit_bindings) {
            /* Force locations and bindings for contexts that do not support
             * explicit locations and bindings */
            ret = ngli_program_gl_set_locations_and_bindings(s->program, s);
            if (ret < 0)
                return ret;
        }
    }
#endif

    return 0;
}

int ngli_pgcraft_get_uniform_index(const struct pgcraft *s, const char *name, int stage)
{
    const struct pgcraft_compat_info *compat_info = &s->compat_info;
    if (compat_info->use_ublocks)
        return get_ublock_index(s, name, stage);
    else
        return get_uniform_index(s, name);
}

const struct darray *ngli_pgcraft_get_texture_infos(const struct pgcraft *s)
{
    return &s->texture_infos;
}

const struct pgcraft_compat_info *ngli_pgcraft_get_compat_info(const struct pgcraft *s)
{
    return &s->compat_info;
}

struct program *ngli_pgcraft_get_program(const struct pgcraft *s)
{
    return s->program;
}

struct pipeline_layout ngli_pgcraft_get_pipeline_layout(const struct pgcraft *s)
{
    const struct pipeline_layout layout = {
        .uniforms_desc   = ngli_darray_data(&s->filtered_pipeline_info.desc.uniforms),
        .nb_uniforms     = ngli_darray_count(&s->filtered_pipeline_info.desc.uniforms),
        .textures_desc   = ngli_darray_data(&s->filtered_pipeline_info.desc.textures),
        .nb_textures     = ngli_darray_count(&s->filtered_pipeline_info.desc.textures),
        .attributes_desc = ngli_darray_data(&s->filtered_pipeline_info.desc.attributes),
        .nb_attributes   = ngli_darray_count(&s->filtered_pipeline_info.desc.attributes),
        .buffers_desc    = ngli_darray_data(&s->filtered_pipeline_info.desc.buffers),
        .nb_buffers      = ngli_darray_count(&s->filtered_pipeline_info.desc.buffers),
    };
    return layout;
}

struct pipeline_resources ngli_pgcraft_get_pipeline_resources(const struct pgcraft *s)
{
    const struct pipeline_resources resources = {
        .uniforms      = ngli_darray_data(&s->filtered_pipeline_info.data.uniforms),
        .nb_uniforms   = ngli_darray_count(&s->filtered_pipeline_info.data.uniforms),
        .textures      = ngli_darray_data(&s->filtered_pipeline_info.data.textures),
        .nb_textures   = ngli_darray_count(&s->filtered_pipeline_info.data.textures),
        .attributes    = ngli_darray_data(&s->filtered_pipeline_info.data.attributes),
        .nb_attributes = ngli_darray_count(&s->filtered_pipeline_info.data.attributes),
        .buffers       = ngli_darray_data(&s->filtered_pipeline_info.data.buffers),
        .nb_buffers    = ngli_darray_count(&s->filtered_pipeline_info.data.buffers),
    };
    return resources;
}

void ngli_pgcraft_freep(struct pgcraft **sp)
{
    struct pgcraft *s = *sp;
    if (!s)
        return;

    ngli_darray_reset(&s->texture_infos);
    ngli_darray_reset(&s->vert_out_vars);

    struct pgcraft_compat_info *compat_info = &s->compat_info;
    for (int i = 0; i < NGLI_ARRAY_NB(compat_info->ublocks); i++) {
        ngli_block_reset(&compat_info->ublocks[i]);
    }

    for (int i = 0; i < NGLI_ARRAY_NB(s->shaders); i++)
        ngli_bstr_freep(&s->shaders[i]);

    ngli_darray_reset(&s->pipeline_info.desc.uniforms);
    ngli_darray_reset(&s->pipeline_info.desc.textures);
    ngli_darray_reset(&s->pipeline_info.desc.buffers);
    ngli_darray_reset(&s->pipeline_info.desc.attributes);

    ngli_darray_reset(&s->pipeline_info.data.uniforms);
    ngli_darray_reset(&s->pipeline_info.data.textures);
    ngli_darray_reset(&s->pipeline_info.data.buffers);
    ngli_darray_reset(&s->pipeline_info.data.attributes);

    ngli_darray_reset(&s->filtered_pipeline_info.desc.uniforms);
    ngli_darray_reset(&s->filtered_pipeline_info.desc.textures);
    ngli_darray_reset(&s->filtered_pipeline_info.desc.buffers);
    ngli_darray_reset(&s->filtered_pipeline_info.desc.attributes);

    ngli_darray_reset(&s->filtered_pipeline_info.data.uniforms);
    ngli_darray_reset(&s->filtered_pipeline_info.data.textures);
    ngli_darray_reset(&s->filtered_pipeline_info.data.buffers);
    ngli_darray_reset(&s->filtered_pipeline_info.data.attributes);

    ngli_freep(sp);
}
