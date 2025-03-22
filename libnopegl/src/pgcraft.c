/*
 * Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2023 Nope Forge
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
#include "hwmap.h"
#include "log.h"
#include "ngpu/ctx.h"
#include "ngpu/format.h"
#include "ngpu/type.h"
#include "pgcraft.h"
#include "utils/hmap.h"
#include "utils/memory.h"
#include "utils/utils.h"

#if defined(BACKEND_GL) || defined(BACKEND_GLES)
#include "ngpu/opengl/ctx_gl.h"
#include "ngpu/opengl/feature_gl.h"
#include "ngpu/opengl/program_gl_utils.h"
#endif

enum {
    NGLI_BINDING_TYPE_UBO,
    NGLI_BINDING_TYPE_SSBO,
    NGLI_BINDING_TYPE_TEXTURE,
    NGLI_BINDING_TYPE_IMAGE,
    NGLI_BINDING_TYPE_NB
};

struct pgcraft_pipeline_info {
    struct {
        struct darray textures;   // ngpu_bindgroup_layout_entry
        struct darray buffers;    // ngpu_bindgroup_layout_entry
        struct darray vertex_buffers; // ngpu_vertex_buffer_layout
    } desc;
    struct {
        struct darray textures;   // ngpu_texture_binding
        struct darray buffers;    // ngpu_buffer_binding
        struct darray vertex_buffers; // ngpu_buffer pointer
    } data;
};

struct pgcraft {
    struct ngpu_ctx *gpu_ctx;

    struct darray texture_infos; // pgcraft_texture_info
    struct darray images; // image pointer
    struct pgcraft_compat_info compat_info;

    struct bstr *shaders[NGPU_PROGRAM_SHADER_NB];

    struct darray symbols;

    struct pgcraft_pipeline_info pipeline_info;

    struct darray vert_out_vars; // pgcraft_iovar
    struct darray textures; // pgcraft_texture

    struct ngpu_program *program;
    struct ngpu_bindgroup_layout *bindgroup_layout;

    int bindings[NGLI_BINDING_TYPE_NB];
    int *next_bindings[NGLI_BINDING_TYPE_NB];
    int next_in_locations[NGPU_PROGRAM_SHADER_NB];
    int next_out_locations[NGPU_PROGRAM_SHADER_NB];

    /* GLSL info */
    int glsl_version;
    const char *glsl_version_suffix;
    const char *sym_vertex_index;
    const char *sym_instance_index;
    int has_in_out_layout_qualifiers;
    int has_precision_qualifiers;
    int has_explicit_bindings;
};

/*
 * Currently unmapped formats: r11f_g11f_b10f, rgb10_a2, rgb10_a2ui
 */
static const struct {
    const char *format;
    const char *prefix;
} image_glsl_format_map[NGPU_FORMAT_NB] = {
    [NGPU_FORMAT_R8_UNORM]             = {"r8", ""},
    [NGPU_FORMAT_R8_SNORM]             = {"r8_snorm", ""},
    [NGPU_FORMAT_R8_UINT]              = {"r8ui", "u"},
    [NGPU_FORMAT_R8_SINT]              = {"r8i", "i"},
    [NGPU_FORMAT_R8G8_UNORM]           = {"rg8", ""},
    [NGPU_FORMAT_R8G8_SNORM]           = {"rg8_snorm", ""},
    [NGPU_FORMAT_R8G8_UINT]            = {"rg8ui", "u"},
    [NGPU_FORMAT_R8G8_SINT]            = {"rg8i", "i"},
    [NGPU_FORMAT_R8G8B8_UNORM]         = {NULL, NULL},
    [NGPU_FORMAT_R8G8B8_SNORM]         = {NULL, NULL},
    [NGPU_FORMAT_R8G8B8_UINT]          = {NULL, NULL},
    [NGPU_FORMAT_R8G8B8_SINT]          = {NULL, NULL},
    [NGPU_FORMAT_R8G8B8_SRGB]          = {NULL, NULL},
    [NGPU_FORMAT_R8G8B8A8_UNORM]       = {"rgba8", ""},
    [NGPU_FORMAT_R8G8B8A8_SNORM]       = {"rgba8_snorm", ""},
    [NGPU_FORMAT_R8G8B8A8_UINT]        = {"rgba8ui", "u"},
    [NGPU_FORMAT_R8G8B8A8_SINT]        = {"rgba8i", "i"},
    [NGPU_FORMAT_R8G8B8A8_SRGB]        = {NULL, NULL},
    [NGPU_FORMAT_B8G8R8A8_UNORM]       = {"rgba8", ""},
    [NGPU_FORMAT_B8G8R8A8_SNORM]       = {"rgba8_snorm", ""},
    [NGPU_FORMAT_B8G8R8A8_UINT]        = {"rgba8ui", "u"},
    [NGPU_FORMAT_B8G8R8A8_SINT]        = {"rgba8i", "i"},
    [NGPU_FORMAT_R16_UNORM]            = {"r16", ""},
    [NGPU_FORMAT_R16_SNORM]            = {"r16_snorm", ""},
    [NGPU_FORMAT_R16_UINT]             = {"r16ui", "u"},
    [NGPU_FORMAT_R16_SINT]             = {"r16i", "i"},
    [NGPU_FORMAT_R16_SFLOAT]           = {"r16f", ""},
    [NGPU_FORMAT_R16G16_UNORM]         = {"rg16", ""},
    [NGPU_FORMAT_R16G16_SNORM]         = {"rg16_snorm", ""},
    [NGPU_FORMAT_R16G16_UINT]          = {"rg16ui", "u"},
    [NGPU_FORMAT_R16G16_SINT]          = {"rg16i", "i"},
    [NGPU_FORMAT_R16G16_SFLOAT]        = {"rg16f", ""},
    [NGPU_FORMAT_R16G16B16_UNORM]      = {NULL, NULL},
    [NGPU_FORMAT_R16G16B16_SNORM]      = {NULL, NULL},
    [NGPU_FORMAT_R16G16B16_UINT]       = {NULL, NULL},
    [NGPU_FORMAT_R16G16B16_SINT]       = {NULL, NULL},
    [NGPU_FORMAT_R16G16B16_SFLOAT]     = {NULL, NULL},
    [NGPU_FORMAT_R16G16B16A16_UNORM]   = {"rgba16", ""},
    [NGPU_FORMAT_R16G16B16A16_SNORM]   = {"rgba16_snorm", ""},
    [NGPU_FORMAT_R16G16B16A16_UINT]    = {"rgba16ui", "u"},
    [NGPU_FORMAT_R16G16B16A16_SINT]    = {"rgba16i", "i"},
    [NGPU_FORMAT_R16G16B16A16_SFLOAT]  = {"rgba16f", ""},
    [NGPU_FORMAT_R32_UINT]             = {"r32ui", "u"},
    [NGPU_FORMAT_R32_SINT]             = {"r32i", "i"},
    [NGPU_FORMAT_R32_SFLOAT]           = {"r32f", ""},
    [NGPU_FORMAT_R32G32_UINT]          = {"rg32ui", "u"},
    [NGPU_FORMAT_R32G32_SINT]          = {"rg32i", "i"},
    [NGPU_FORMAT_R32G32_SFLOAT]        = {"rg32f", ""},
    [NGPU_FORMAT_R32G32B32_UINT]       = {NULL, NULL},
    [NGPU_FORMAT_R32G32B32_SINT]       = {NULL, NULL},
    [NGPU_FORMAT_R32G32B32_SFLOAT]     = {NULL, NULL},
    [NGPU_FORMAT_R32G32B32A32_UINT]    = {"rgba32ui", "u"},
    [NGPU_FORMAT_R32G32B32A32_SINT]    = {"rgba32i", "i"},
    [NGPU_FORMAT_R32G32B32A32_SFLOAT]  = {"rgba32f", ""},
    [NGPU_FORMAT_D16_UNORM]            = {NULL, NULL},
    [NGPU_FORMAT_X8_D24_UNORM_PACK32]  = {NULL, NULL},
    [NGPU_FORMAT_D32_SFLOAT]           = {NULL, NULL},
    [NGPU_FORMAT_D24_UNORM_S8_UINT]    = {NULL, NULL},
    [NGPU_FORMAT_D32_SFLOAT_S8_UINT]   = {NULL, NULL},
    [NGPU_FORMAT_S8_UINT]              = {NULL, NULL},
};

enum {
    TYPE_FLAG_IS_SAMPLER          = 1 << 0,
    TYPE_FLAG_HAS_PRECISION       = 1 << 1,
    TYPE_FLAG_IS_INT              = 1 << 2,
    TYPE_FLAG_IS_IMAGE            = 1 << 3,
};

static const int type_flags_map[NGPU_TYPE_NB] = {
    [NGPU_TYPE_I32]                         = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_INT,
    [NGPU_TYPE_IVEC2]                       = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_INT,
    [NGPU_TYPE_IVEC3]                       = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_INT,
    [NGPU_TYPE_IVEC4]                       = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_INT,
    [NGPU_TYPE_U32]                         = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_INT,
    [NGPU_TYPE_UVEC2]                       = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_INT,
    [NGPU_TYPE_UVEC3]                       = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_INT,
    [NGPU_TYPE_UVEC4]                       = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_INT,
    [NGPU_TYPE_F32]                         = TYPE_FLAG_HAS_PRECISION,
    [NGPU_TYPE_VEC2]                        = TYPE_FLAG_HAS_PRECISION,
    [NGPU_TYPE_VEC3]                        = TYPE_FLAG_HAS_PRECISION,
    [NGPU_TYPE_VEC4]                        = TYPE_FLAG_HAS_PRECISION,
    [NGPU_TYPE_MAT3]                        = TYPE_FLAG_HAS_PRECISION,
    [NGPU_TYPE_MAT4]                        = TYPE_FLAG_HAS_PRECISION,
    [NGPU_TYPE_BOOL]                        = 0,
    [NGPU_TYPE_SAMPLER_2D]                  = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_SAMPLER,
    [NGPU_TYPE_SAMPLER_2D_ARRAY]            = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_SAMPLER,
    [NGPU_TYPE_SAMPLER_2D_RECT]             = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_SAMPLER,
    [NGPU_TYPE_SAMPLER_3D]                  = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_SAMPLER,
    [NGPU_TYPE_SAMPLER_CUBE]                = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_SAMPLER,
    [NGPU_TYPE_SAMPLER_EXTERNAL_OES]        = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_SAMPLER,
    [NGPU_TYPE_SAMPLER_EXTERNAL_2D_Y2Y_EXT] = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_SAMPLER,
    [NGPU_TYPE_IMAGE_2D]                    = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_IMAGE,
    [NGPU_TYPE_IMAGE_2D_ARRAY]              = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_IMAGE,
    [NGPU_TYPE_IMAGE_3D]                    = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_IMAGE,
    [NGPU_TYPE_IMAGE_CUBE]                  = TYPE_FLAG_HAS_PRECISION|TYPE_FLAG_IS_IMAGE,
    [NGPU_TYPE_UNIFORM_BUFFER]              = 0,
    [NGPU_TYPE_UNIFORM_BUFFER_DYNAMIC]      = 0,
    [NGPU_TYPE_STORAGE_BUFFER]              = 0,
    [NGPU_TYPE_STORAGE_BUFFER_DYNAMIC]      = 0,
};

static const int type_binding_map[NGPU_TYPE_NB] = {
    [NGPU_TYPE_SAMPLER_2D]                  = NGLI_BINDING_TYPE_TEXTURE,
    [NGPU_TYPE_SAMPLER_2D_ARRAY]            = NGLI_BINDING_TYPE_TEXTURE,
    [NGPU_TYPE_SAMPLER_2D_RECT]             = NGLI_BINDING_TYPE_TEXTURE,
    [NGPU_TYPE_SAMPLER_3D]                  = NGLI_BINDING_TYPE_TEXTURE,
    [NGPU_TYPE_SAMPLER_CUBE]                = NGLI_BINDING_TYPE_TEXTURE,
    [NGPU_TYPE_SAMPLER_EXTERNAL_OES]        = NGLI_BINDING_TYPE_TEXTURE,
    [NGPU_TYPE_SAMPLER_EXTERNAL_2D_Y2Y_EXT] = NGLI_BINDING_TYPE_TEXTURE,
    [NGPU_TYPE_IMAGE_2D]                    = NGLI_BINDING_TYPE_IMAGE,
    [NGPU_TYPE_IMAGE_2D_ARRAY]              = NGLI_BINDING_TYPE_IMAGE,
    [NGPU_TYPE_IMAGE_3D]                    = NGLI_BINDING_TYPE_IMAGE,
    [NGPU_TYPE_IMAGE_CUBE]                  = NGLI_BINDING_TYPE_IMAGE,
    [NGPU_TYPE_UNIFORM_BUFFER]              = NGLI_BINDING_TYPE_UBO,
    [NGPU_TYPE_UNIFORM_BUFFER_DYNAMIC]      = NGLI_BINDING_TYPE_UBO,
    [NGPU_TYPE_STORAGE_BUFFER]              = NGLI_BINDING_TYPE_SSBO,
    [NGPU_TYPE_STORAGE_BUFFER_DYNAMIC]      = NGLI_BINDING_TYPE_SSBO,
};

static int is_sampler(enum ngpu_type type)
{
    return type_flags_map[type] & TYPE_FLAG_IS_SAMPLER;
}

static int is_image(enum ngpu_type type)
{
    return type_flags_map[type] & TYPE_FLAG_IS_IMAGE;
}

static int type_has_precision(enum ngpu_type type)
{
    return type_flags_map[type] & TYPE_FLAG_HAS_PRECISION;
}

static int type_is_int(enum ngpu_type type)
{
    return type_flags_map[type] & TYPE_FLAG_IS_INT;
}

static const char *get_glsl_type(enum ngpu_type type)
{
    const char *ret = ngpu_type_get_name(type);
    ngli_assert(ret);
    return ret;
}

static int request_next_binding(struct pgcraft *s, enum ngpu_type type)
{
    ngli_assert(type >= 0 && type < NGPU_TYPE_NB);
    const int binding_type = type_binding_map[type];

    int *next_bind = s->next_bindings[binding_type];
    ngli_assert(next_bind);

    return (*next_bind)++;
}

static const char *get_precision_qualifier(const struct pgcraft *s, enum ngpu_type type, enum ngpu_precision precision, const char *defaultp)
{
    if (!s->has_precision_qualifiers || !type_has_precision(type))
        return "";
    static const char *precision_qualifiers[NGPU_PRECISION_NB] = {
        [NGPU_PRECISION_AUTO]   = NULL,
        [NGPU_PRECISION_HIGH]   = "highp",
        [NGPU_PRECISION_MEDIUM] = "mediump",
        [NGPU_PRECISION_LOW]    = "lowp",
    };
    const char *ret = precision_qualifiers[precision];
    return ret ? ret : defaultp;
}

static const char *get_array_suffix(size_t count, char *buf, size_t len)
{
    if (count == NGPU_BLOCK_DESC_VARIADIC_COUNT)
        snprintf(buf, len, "[]");
    else if (count > 0)
        snprintf(buf, len, "[%zu]", count);
    return buf;
}

#define GET_ARRAY_SUFFIX(count) get_array_suffix(count, (char[32]){0}, 32)

static int inject_block_uniform(struct pgcraft *s, struct bstr *b,
                                const struct pgcraft_uniform *uniform, int stage)
{
    struct pgcraft_compat_info *compat_info = &s->compat_info;
    struct ngpu_block_desc *block = &compat_info->ublocks[stage];

    return ngpu_block_desc_add_field(block, uniform->name, uniform->type, uniform->count);
}

static int inject_uniform(struct pgcraft *s, struct bstr *b,
                          const struct pgcraft_uniform *uniform)
{
    return inject_block_uniform(s, b, uniform, uniform->stage);
}

static int inject_uniforms(struct pgcraft *s, struct bstr *b,
                           const struct pgcraft_params *params, int stage)
{
    for (size_t i = 0; i < params->nb_uniforms; i++) {
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

static const enum ngpu_type texture_types_map[NGLI_PGCRAFT_SHADER_TEX_TYPE_NB][NGLI_INFO_FIELD_NB] = {
    [NGLI_PGCRAFT_SHADER_TEX_TYPE_VIDEO] = {
        [NGLI_INFO_FIELD_COORDINATE_MATRIX] = NGPU_TYPE_MAT4,
        [NGLI_INFO_FIELD_DIMENSIONS]        = NGPU_TYPE_VEC2,
        [NGLI_INFO_FIELD_TIMESTAMP]         = NGPU_TYPE_F32,
        [NGLI_INFO_FIELD_COLOR_MATRIX]      = NGPU_TYPE_MAT4,
        [NGLI_INFO_FIELD_SAMPLING_MODE]     = NGPU_TYPE_I32,
        [NGLI_INFO_FIELD_SAMPLER_0]         = NGPU_TYPE_SAMPLER_2D,
        [NGLI_INFO_FIELD_SAMPLER_1]         = NGPU_TYPE_SAMPLER_2D,
        [NGLI_INFO_FIELD_SAMPLER_2]         = NGPU_TYPE_SAMPLER_2D,
#if defined(TARGET_ANDROID)
        [NGLI_INFO_FIELD_SAMPLER_OES]       = NGPU_TYPE_SAMPLER_EXTERNAL_OES,
#elif defined(TARGET_DARWIN)
        [NGLI_INFO_FIELD_SAMPLER_RECT_0]    = NGPU_TYPE_SAMPLER_2D_RECT,
        [NGLI_INFO_FIELD_SAMPLER_RECT_1]    = NGPU_TYPE_SAMPLER_2D_RECT,
#endif
    },
    [NGLI_PGCRAFT_SHADER_TEX_TYPE_2D] = {
        [NGLI_INFO_FIELD_SAMPLER_0]         = NGPU_TYPE_SAMPLER_2D,
        [NGLI_INFO_FIELD_COORDINATE_MATRIX] = NGPU_TYPE_MAT4,
        [NGLI_INFO_FIELD_DIMENSIONS]        = NGPU_TYPE_VEC2,
        [NGLI_INFO_FIELD_TIMESTAMP]         = NGPU_TYPE_F32,
    },
    [NGLI_PGCRAFT_SHADER_TEX_TYPE_2D_ARRAY] = {
        [NGLI_INFO_FIELD_SAMPLER_0]         = NGPU_TYPE_SAMPLER_2D_ARRAY,
        [NGLI_INFO_FIELD_COORDINATE_MATRIX] = NGPU_TYPE_MAT4,
    },
    [NGLI_PGCRAFT_SHADER_TEX_TYPE_IMAGE_2D] = {
        [NGLI_INFO_FIELD_SAMPLER_0]         = NGPU_TYPE_IMAGE_2D,
        [NGLI_INFO_FIELD_COORDINATE_MATRIX] = NGPU_TYPE_MAT4,
        [NGLI_INFO_FIELD_DIMENSIONS]        = NGPU_TYPE_VEC2,
        [NGLI_INFO_FIELD_TIMESTAMP]         = NGPU_TYPE_F32,
    },
    [NGLI_PGCRAFT_SHADER_TEX_TYPE_IMAGE_2D_ARRAY] = {
        [NGLI_INFO_FIELD_SAMPLER_0]         = NGPU_TYPE_IMAGE_2D_ARRAY,
        [NGLI_INFO_FIELD_COORDINATE_MATRIX] = NGPU_TYPE_MAT4,
    },
    [NGLI_PGCRAFT_SHADER_TEX_TYPE_3D] = {
        [NGLI_INFO_FIELD_SAMPLER_0]         = NGPU_TYPE_SAMPLER_3D,
        [NGLI_INFO_FIELD_COORDINATE_MATRIX] = NGPU_TYPE_MAT4,
    },
    [NGLI_PGCRAFT_SHADER_TEX_TYPE_IMAGE_3D] = {
        [NGLI_INFO_FIELD_SAMPLER_0]         = NGPU_TYPE_IMAGE_3D,
        [NGLI_INFO_FIELD_COORDINATE_MATRIX] = NGPU_TYPE_MAT4,
    },
    [NGLI_PGCRAFT_SHADER_TEX_TYPE_CUBE] = {
        [NGLI_INFO_FIELD_SAMPLER_0]         = NGPU_TYPE_SAMPLER_CUBE,
    },
    [NGLI_PGCRAFT_SHADER_TEX_TYPE_IMAGE_CUBE] = {
        [NGLI_INFO_FIELD_SAMPLER_0]         = NGPU_TYPE_IMAGE_CUBE,
    },
};

static int is_type_supported(struct pgcraft *s, enum ngpu_type type)
{
    const struct ngpu_ctx *gpu_ctx = s->gpu_ctx;
    const struct ngl_config *config = &gpu_ctx->config;

    switch(type) {
    case NGPU_TYPE_SAMPLER_2D_RECT:
        return ngli_hwmap_is_image_layout_supported(config->backend, NGLI_IMAGE_LAYOUT_RECTANGLE) ||
               ngli_hwmap_is_image_layout_supported(config->backend, NGLI_IMAGE_LAYOUT_NV12_RECTANGLE);
    case NGPU_TYPE_SAMPLER_EXTERNAL_OES:
    case NGPU_TYPE_SAMPLER_EXTERNAL_2D_Y2Y_EXT:
        return ngli_hwmap_is_image_layout_supported(config->backend, NGLI_IMAGE_LAYOUT_MEDIACODEC);
    default:
        return 1;
    }
}

static int prepare_texture_info_fields(struct pgcraft *s, const struct pgcraft_params *params, int graphics,
                                        const struct pgcraft_texture *texture,
                                        struct pgcraft_texture_info *info)
{
    const enum ngpu_type *types_map = texture_types_map[texture->type];

    for (size_t i = 0; i < NGLI_INFO_FIELD_NB; i++) {
        struct pgcraft_texture_info_field *field = &info->fields[i];
        const enum ngpu_type type = types_map[i];
        if (type == NGPU_TYPE_NONE || !is_type_supported(s, type))
            continue;
        field->type = type;
        if (graphics && i == NGLI_INFO_FIELD_COORDINATE_MATRIX)
            field->stage = NGPU_PROGRAM_SHADER_VERT;
        else
            field->stage = texture->stage;
    }
    return 0;
}

static int prepare_texture_infos(struct pgcraft *s, const struct pgcraft_params *params, int graphics)
{
    ngli_darray_init(&s->textures, sizeof(struct pgcraft_texture), 0);
    for (size_t i = 0; i < params->nb_textures; i++) {
        const struct pgcraft_texture *texture = &params->textures[i];
        ngli_assert(!(texture->type == NGLI_PGCRAFT_SHADER_TEX_TYPE_VIDEO && texture->texture));

        if (!ngli_darray_push(&s->textures, &params->textures[i]))
            return NGL_ERROR_MEMORY;

        if (!ngli_darray_push(&s->symbols, texture->name))
            return NGL_ERROR_MEMORY;

        struct pgcraft_texture_info info = {.id = ngli_darray_count(&s->symbols) - 1};
        int ret = prepare_texture_info_fields(s, params, graphics, texture, &info);
        if (ret < 0)
            return ret;

        if (!ngli_darray_push(&s->texture_infos, &info))
            return NGL_ERROR_MEMORY;

        if (!ngli_darray_push(&s->images, &texture->image))
            return NGL_ERROR_MEMORY;
    }
    return 0;
}

static int inject_texture(struct pgcraft *s, const struct pgcraft_texture *texture, const struct pgcraft_texture_info *info, int stage)
{
    for (size_t i = 0; i < NGLI_INFO_FIELD_NB; i++) {
        const struct pgcraft_texture_info_field *field = &info->fields[i];

        if (field->type == NGPU_TYPE_NONE || field->stage != stage)
            continue;

        char name[MAX_ID_LEN];
        int len = snprintf(name, sizeof(name), "%s%s", texture->name, texture_info_suffixes[i]);
        if (len >= sizeof(name)) {
            LOG(ERROR, "texture name \"%s\" is too long", texture->name);
            return NGL_ERROR_MEMORY;
        }

        struct bstr *b = s->shaders[stage];

        if (is_sampler(field->type) || is_image(field->type)) {
            if (!ngli_darray_push(&s->symbols, name))
                return NGL_ERROR_MEMORY;

            const struct ngpu_bindgroup_layout_entry layout_entry = {
                .id          = ngli_darray_count(&s->symbols) - 1,
                .type        = field->type,
                .binding     = request_next_binding(s, field->type),
                .access      = texture->writable ? NGPU_ACCESS_READ_WRITE : NGPU_ACCESS_READ_BIT,
                .stage_flags = 1U << stage,
            };

            const char *prefix = "";
            if (is_image(field->type)) {
                if (texture->format == NGPU_FORMAT_UNDEFINED) {
                    LOG(ERROR, "texture format must be set when accessing it as an image");
                    return NGL_ERROR_INVALID_ARG;
                }
                const char *format = image_glsl_format_map[texture->format].format;
                prefix = image_glsl_format_map[texture->format].prefix;
                if (!format || !prefix) {
                    LOG(ERROR, "unsupported texture format");
                    return NGL_ERROR_UNSUPPORTED;
                }

                ngli_bstr_printf(b, "layout(%s", format);

                if (s->has_explicit_bindings)
                    ngli_bstr_printf(b, ", binding=%d", layout_entry.binding);

                /*
                 * Restrict memory qualifier according to the OpenGLES 3.2 spec
                 * (Section 4.10. Memory qualifiers):
                 *
                 *     Except for image variables qualified with the format
                 *     qualifiers r32f, r32i, and r32ui, image variables must
                 *     specify a memory qualifier (readonly, writeonly, or both).
                 */
                const char *writable_qualifier= "";
                if (texture->format != NGPU_FORMAT_R32_SFLOAT &&
                    texture->format != NGPU_FORMAT_R32_SINT &&
                    texture->format != NGPU_FORMAT_R32_UINT) {
                    writable_qualifier = "writeonly";
                }
                ngli_bstr_printf(b, ") %s ", texture->writable ? writable_qualifier : "readonly");
            } else if (s->has_explicit_bindings) {
                ngli_bstr_printf(b, "layout(binding=%d) ", layout_entry.binding);
            }

            const char *type = get_glsl_type(field->type);
            const char *precision = get_precision_qualifier(s, field->type, texture->precision, "lowp");
            ngli_bstr_printf(b, "uniform %s %s%s %s;\n", precision, prefix, type, name);

            if (!ngli_darray_push(&s->pipeline_info.desc.textures, &layout_entry))
                return NGL_ERROR_MEMORY;

            const struct ngpu_texture_binding texture_binding = {
                .texture = texture->texture,
            };
            if (!ngli_darray_push(&s->pipeline_info.data.textures, &texture_binding))
                return NGL_ERROR_MEMORY;
        } else {
            struct pgcraft_uniform uniform = {
                .stage = field->stage,
                .type = field->type,
            };
            snprintf(uniform.name, sizeof(uniform.name), "%s", name);
            int ret = inject_uniform(s, b, &uniform);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

static int inject_textures(struct pgcraft *s, const struct pgcraft_params *params, int stage)
{
    const struct pgcraft_texture *textures = ngli_darray_data(&s->textures);
    const struct pgcraft_texture_info *texture_infos = ngli_darray_data(&s->texture_infos);
    for (size_t i = 0; i < ngli_darray_count(&s->texture_infos); i++) {
        const struct pgcraft_texture_info *info = &texture_infos[i];
        int ret = inject_texture(s, &textures[i], info, stage);
        if (ret < 0)
            return ret;
    }
    return 0;
}

static const char *glsl_layout_str_map[NGPU_BLOCK_NB_LAYOUTS] = {
    [NGPU_BLOCK_LAYOUT_STD140] = "std140",
    [NGPU_BLOCK_LAYOUT_STD430] = "std430",
};

static int inject_block(struct pgcraft *s, struct bstr *b,
                        const struct pgcraft_block *named_block)
{
    if (!ngli_darray_push(&s->symbols, named_block->name))
        return NGL_ERROR_MEMORY;

    const struct ngpu_bindgroup_layout_entry layout_entry = {
        .id          = ngli_darray_count(&s->symbols) - 1,
        .type        = named_block->type,
        .binding     = request_next_binding(s, named_block->type),
        .access      = named_block->writable ? NGPU_ACCESS_READ_WRITE : NGPU_ACCESS_READ_BIT,
        .stage_flags = 1U << named_block->stage,
    };

    const struct ngpu_block_desc *block = named_block->block;
    const char *layout = glsl_layout_str_map[block->layout];
    if (s->has_explicit_bindings) {
        ngli_bstr_printf(b, "layout(%s,binding=%d)", layout, layout_entry.binding);
    } else {
        ngli_bstr_printf(b, "layout(%s)", layout);
    }

    if (named_block->type == NGPU_TYPE_STORAGE_BUFFER && !named_block->writable)
        ngli_bstr_print(b, " readonly");

    const char *keyword = get_glsl_type(named_block->type);
    ngli_bstr_printf(b, " %s %s_block {\n", keyword, named_block->name);
    const struct ngpu_block_field *field_info = ngli_darray_data(&block->fields);
    for (size_t i = 0; i < ngli_darray_count(&block->fields); i++) {
        const struct ngpu_block_field *fi = &field_info[i];
        const char *type = get_glsl_type(fi->type);
        const char *precision = get_precision_qualifier(s, fi->type, fi->precision, "");
        const char *array_suffix = GET_ARRAY_SUFFIX(fi->count);
        ngli_bstr_printf(b, "    %s %s %s%s;\n", precision, type, fi->name, array_suffix);
    }
    const char *instance_name = named_block->instance_name ? named_block->instance_name : named_block->name;
    ngli_bstr_printf(b, "} %s;\n", instance_name);

    if (!ngli_darray_push(&s->pipeline_info.desc.buffers, &layout_entry))
        return NGL_ERROR_MEMORY;

    if (!ngli_darray_push(&s->pipeline_info.data.buffers, &named_block->buffer))
        return NGL_ERROR_MEMORY;
    return layout_entry.binding;
}

static int inject_blocks(struct pgcraft *s, struct bstr *b,
                         const struct pgcraft_params *params, int stage)
{
    for (size_t i = 0; i < params->nb_blocks; i++) {
        const struct pgcraft_block *block = &params->blocks[i];
        if (block->stage != stage)
            continue;
        int ret = inject_block(s, b, &params->blocks[i]);
        if (ret < 0)
            return ret;
    }
    return 0;
}

static int get_location_count(enum ngpu_type type)
{
    switch (type) {
    case NGPU_TYPE_MAT3: return 3;
    case NGPU_TYPE_MAT4: return 4;
    default:             return 1;
    }
}

static int inject_attribute(struct pgcraft *s, struct bstr *b,
                            const struct pgcraft_attribute *attribute)
{
    const char *type = get_glsl_type(attribute->type);
    const int attribute_count = get_location_count(attribute->type);

    const int base_location = s->next_in_locations[NGPU_PROGRAM_SHADER_VERT];
    s->next_in_locations[NGPU_PROGRAM_SHADER_VERT] += attribute_count;

    if (s->has_in_out_layout_qualifiers) {
        ngli_bstr_printf(b, "layout(location=%d) ", base_location);
    }

    const char *precision = get_precision_qualifier(s, attribute->type, attribute->precision, "highp");
    ngli_bstr_printf(b, "in %s %s %s;\n", precision, type, attribute->name);

    struct ngpu_vertex_buffer_layout vertex_buffer = {
        .stride = attribute->stride,
        .rate = attribute->rate,
    };

    const int attribute_offset = ngpu_format_get_bytes_per_pixel(attribute->format);
    for (int i = 0; i < attribute_count; i++) {
        if (!ngli_darray_push(&s->symbols, attribute->name))
            return NGL_ERROR_MEMORY;

        ngli_assert(vertex_buffer.nb_attributes < NGPU_MAX_ATTRIBUTES_PER_BUFFER);
        vertex_buffer.attributes[vertex_buffer.nb_attributes++] = (struct ngpu_vertex_attribute) {
            .id = ngli_darray_count(&s->symbols) - 1,
            .location = base_location + i,
            .format = attribute->format,
            .offset = attribute->offset + i * attribute_offset,
        };
    }

    if (!ngli_darray_push(&s->pipeline_info.desc.vertex_buffers, &vertex_buffer))
        return NGL_ERROR_MEMORY;
    if (!ngli_darray_push(&s->pipeline_info.data.vertex_buffers, &attribute->buffer))
        return NGL_ERROR_MEMORY;

    return 0;
}

static int inject_attributes(struct pgcraft *s, struct bstr *b,
                             const struct pgcraft_params *params)
{
    for (size_t i = 0; i < params->nb_attributes; i++) {
        int ret = inject_attribute(s, b, &params->attributes[i]);
        if (ret < 0)
            return ret;
    }
    return 0;
}

const char *ublock_names[] = {
    [NGPU_PROGRAM_SHADER_VERT] = "vert",
    [NGPU_PROGRAM_SHADER_FRAG] = "frag",
    [NGPU_PROGRAM_SHADER_COMP] = "comp",
};

static int inject_ublock(struct pgcraft *s, struct bstr *b, int stage)
{
    struct pgcraft_compat_info *compat_info = &s->compat_info;

    struct ngpu_block_desc *block = &compat_info->ublocks[stage];
    const size_t block_size = ngpu_block_desc_get_size(block, 0);
    if (!block_size)
        return 0;

    struct pgcraft_block pgcraft_block = {
        /* instance name is empty to make field accesses identical to uniform accesses */
        .instance_name = "",
        .type          = NGPU_TYPE_UNIFORM_BUFFER,
        .stage         = stage,
        .block         = block,
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
    for (size_t i = 0; i < params->nb_blocks; i++) {
        const struct pgcraft_block *pgcraft_block = &params->blocks[i];
        if (pgcraft_block->stage == stage && pgcraft_block->type == NGPU_TYPE_STORAGE_BUFFER)
            return 1;
    }
    return 0;
}

static int params_have_images(struct pgcraft *s, const struct pgcraft_params *params, int stage)
{
    const struct darray *texture_infos_array = &s->texture_infos;
    const struct pgcraft_texture_info *texture_infos = ngli_darray_data(texture_infos_array);
    for (size_t i = 0; i < ngli_darray_count(texture_infos_array); i++) {
        const struct pgcraft_texture_info *info = &texture_infos[i];
        for (size_t j = 0; j < NGLI_INFO_FIELD_NB; j++) {
            const struct pgcraft_texture_info_field *field = &info->fields[j];
            if (field->stage == stage &&
                (field->type == NGPU_TYPE_IMAGE_2D ||
                 field->type == NGPU_TYPE_IMAGE_2D_ARRAY ||
                 field->type == NGPU_TYPE_IMAGE_3D ||
                 field->type == NGPU_TYPE_IMAGE_CUBE))
                return 1;
        }
    }
    return 0;
}

static void set_glsl_header(struct pgcraft *s, struct bstr *b, const struct pgcraft_params *params, int stage)
{
    struct ngpu_ctx *gpu_ctx = s->gpu_ctx;
    const struct ngl_config *config = &gpu_ctx->config;

    ngli_bstr_printf(b, "#version %d%s\n", s->glsl_version, s->glsl_version_suffix);

    const int require_ssbo_feature = params_have_ssbos(s, params, stage);
    const int require_image_feature = params_have_images(s, params, stage);
#if defined(TARGET_ANDROID)
    const int require_image_external_essl3_feature = ngli_darray_count(&s->texture_infos) > 0;
#endif

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
        {NGL_BACKEND_OPENGL, "GL_ARB_compute_shader",                 430, stage == NGPU_PROGRAM_SHADER_COMP},

        /* OpenGLES */
#if defined(TARGET_ANDROID)
        {NGL_BACKEND_OPENGLES, "GL_OES_EGL_image_external_essl3", INT_MAX, require_image_external_essl3_feature},
#endif
    };

    for (size_t i = 0; i < NGLI_ARRAY_NB(features); i++) {
        if (features[i].backend == config->backend &&
            features[i].glsl_version > s->glsl_version &&
            features[i].required)
            ngli_bstr_printf(b, "#extension %s : require\n", features[i].extension);
    }

    ngli_bstr_print(b, "\n");
}

static int texture_needs_clamping(const struct pgcraft_params *params,
                                  const char *name, size_t name_len)
{
    for (size_t i = 0; i < params->nb_textures; i++) {
        const struct pgcraft_texture *pgcraft_texture = &params->textures[i];
        if (!strncmp(name, pgcraft_texture->name, name_len))
            return pgcraft_texture->clamp_video;
    }
    return 0;
}

static enum pgcraft_shader_tex_type get_texture_type(const struct pgcraft_params *params,
                                                     const char *name, size_t name_len)
{
    for (size_t i = 0; i < params->nb_textures; i++) {
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
    struct ngpu_ctx *gpu_ctx = s->gpu_ctx;
    const struct ngl_config *config = &gpu_ctx->config;

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
    size_t arg0_len = p - arg0_start;

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
            ngli_bstr_printf(dst, "texture(%.*s, %.*s)", ARG_FMT(arg0), ARG_FMT(coords));
            ngli_bstr_print(dst, p);
            return 0;
        }

        const int clamp = texture_needs_clamping(params, arg0_start, arg0_len);
        if (clamp)
            ngli_bstr_print(dst, "clamp(");

        ngli_bstr_print(dst, "(");

        if (ngli_hwmap_is_image_layout_supported(config->backend, NGLI_IMAGE_LAYOUT_MEDIACODEC)) {
            ngli_bstr_printf(dst, "%.*s_sampling_mode == %d ? ", ARG_FMT(arg0), NGLI_IMAGE_LAYOUT_MEDIACODEC);
            ngli_bstr_printf(dst, "texture(%.*s_oes, %.*s) : ", ARG_FMT(arg0), ARG_FMT(coords));
        }

        if (ngli_hwmap_is_image_layout_supported(config->backend, NGLI_IMAGE_LAYOUT_NV12_RECTANGLE)) {
            ngli_bstr_printf(dst, " %.*s_sampling_mode == %d ? ", ARG_FMT(arg0), NGLI_IMAGE_LAYOUT_NV12_RECTANGLE);
            ngli_bstr_printf(dst, "%.*s_color_matrix * vec4(texture(%.*s_rect_0, (%.*s) * textureSize(%.*s_rect_0)).r, "
                                                           "texture(%.*s_rect_1, (%.*s) * textureSize(%.*s_rect_1)).rg, 1.0) : ",
                             ARG_FMT(arg0),
                             ARG_FMT(arg0), ARG_FMT(coords), ARG_FMT(arg0),
                             ARG_FMT(arg0), ARG_FMT(coords), ARG_FMT(arg0));
        }

        if (ngli_hwmap_is_image_layout_supported(config->backend, NGLI_IMAGE_LAYOUT_RECTANGLE)) {
            ngli_bstr_printf(dst, "%.*s_sampling_mode == %d ? ", ARG_FMT(arg0), NGLI_IMAGE_LAYOUT_RECTANGLE);
            ngli_bstr_printf(dst, "texture(%.*s_rect_0, (%.*s) * textureSize(%.*s_rect_0)) : ",
                             ARG_FMT(arg0), ARG_FMT(coords), ARG_FMT(arg0));
        }

        if (ngli_hwmap_is_image_layout_supported(config->backend, NGLI_IMAGE_LAYOUT_NV12)) {
            ngli_bstr_printf(dst, "%.*s_sampling_mode == %d ? ", ARG_FMT(arg0), NGLI_IMAGE_LAYOUT_NV12);
            ngli_bstr_printf(dst, "%.*s_color_matrix * vec4(texture(%.*s,   %.*s).r, "
                                                           "texture(%.*s_1, %.*s).rg, 1.0) : ",
                             ARG_FMT(arg0),
                             ARG_FMT(arg0), ARG_FMT(coords),
                             ARG_FMT(arg0), ARG_FMT(coords));
        }

        if (ngli_hwmap_is_image_layout_supported(config->backend, NGLI_IMAGE_LAYOUT_YUV)) {
            ngli_bstr_printf(dst, "%.*s_sampling_mode == %d ? ", ARG_FMT(arg0), NGLI_IMAGE_LAYOUT_YUV);
            ngli_bstr_printf(dst, "%.*s_color_matrix * vec4(texture(%.*s,   %.*s).r, "
                                                           "texture(%.*s_1, %.*s).r, "
                                                           "texture(%.*s_2, %.*s).r, 1.0) : ",
                             ARG_FMT(arg0),
                             ARG_FMT(arg0), ARG_FMT(coords),
                             ARG_FMT(arg0), ARG_FMT(coords),
                             ARG_FMT(arg0), ARG_FMT(coords));
        }

        if (ngli_hwmap_is_image_layout_supported(config->backend, NGLI_IMAGE_LAYOUT_DEFAULT)) {
            ngli_bstr_printf(dst, "texture(%.*s, %.*s)", ARG_FMT(arg0), ARG_FMT(coords));
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
    const size_t nb_tokens = ngli_darray_count(&token_stack);
    for (size_t i = 0; i < nb_tokens; i++) {
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
    static const char *qualifiers[2] = {
        [NGPU_PROGRAM_SHADER_VERT] = "out",
        [NGPU_PROGRAM_SHADER_FRAG] = "in",
    };
    const char *qualifier = qualifiers[stage];
    const struct pgcraft_iovar *iovars = ngli_darray_data(&s->vert_out_vars);
    int location = 0;
    for (size_t i = 0; i < ngli_darray_count(&s->vert_out_vars); i++) {
        if (s->has_in_out_layout_qualifiers)
            ngli_bstr_printf(b, "layout(location=%d) ", location);
        const struct pgcraft_iovar *iovar = &iovars[i];
        const char *precision = stage == NGPU_PROGRAM_SHADER_VERT
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
    struct bstr *b = s->shaders[NGPU_PROGRAM_SHADER_VERT];

    set_glsl_header(s, b, params, NGPU_PROGRAM_SHADER_VERT);

    ngli_bstr_printf(b, "#define ngl_out_pos gl_Position\n"
                        "#define ngl_vertex_index %s\n"
                        "#define ngl_instance_index %s\n",
                        s->sym_vertex_index, s->sym_instance_index);

    int ret;
    if ((ret = inject_iovars(s, b, NGPU_PROGRAM_SHADER_VERT)) < 0 ||
        (ret = inject_uniforms(s, b, params, NGPU_PROGRAM_SHADER_VERT)) < 0 ||
        (ret = inject_textures(s, params, NGPU_PROGRAM_SHADER_VERT)) < 0 ||
        (ret = inject_blocks(s, b, params, NGPU_PROGRAM_SHADER_VERT)) < 0 ||
        (ret = inject_attributes(s, b, params)) < 0 ||
        (ret = inject_ublock(s, b, NGPU_PROGRAM_SHADER_VERT)) < 0)
        return ret;

    ngli_bstr_print(b, params->vert_base);
    return samplers_preproc(s, params, b);
}

static int craft_frag(struct pgcraft *s, const struct pgcraft_params *params)

{
    struct bstr *b = s->shaders[NGPU_PROGRAM_SHADER_FRAG];

    set_glsl_header(s, b, params, NGPU_PROGRAM_SHADER_FRAG);

    if (s->has_precision_qualifiers)
        ngli_bstr_print(b, "#if GL_FRAGMENT_PRECISION_HIGH\n"
                           "precision highp float;\n"
                           "precision highp int;\n"
                           "#else\n"
                           "precision mediump float;\n"
                           "precision mediump int;\n"
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

    if (s->has_in_out_layout_qualifiers) {
        const int out_location = s->next_out_locations[NGPU_PROGRAM_SHADER_FRAG]++;
        ngli_bstr_printf(b, "layout(location=%d) ", out_location);
    }
    if (params->nb_frag_output)
        ngli_bstr_printf(b, "out vec4 ngl_out_color[%zu];\n", params->nb_frag_output);
    else
        ngli_bstr_print(b, "out vec4 ngl_out_color;\n");

    int ret;
    if ((ret = inject_iovars(s, b, NGPU_PROGRAM_SHADER_FRAG)) < 0 ||
        (ret = inject_uniforms(s, b, params, NGPU_PROGRAM_SHADER_FRAG)) < 0 ||
        (ret = inject_textures(s, params, NGPU_PROGRAM_SHADER_FRAG)) < 0 ||
        (ret = inject_blocks(s, b, params, NGPU_PROGRAM_SHADER_FRAG)) < 0 ||
        (ret = inject_ublock(s, b, NGPU_PROGRAM_SHADER_FRAG)) < 0)
        return ret;

    ngli_bstr_print(b, "\n");

    ngli_bstr_print(b, params->frag_base);
    return samplers_preproc(s, params, b);
}

static int craft_comp(struct pgcraft *s, const struct pgcraft_params *params)
{
    struct bstr *b = s->shaders[NGPU_PROGRAM_SHADER_COMP];

    set_glsl_header(s, b, params, NGPU_PROGRAM_SHADER_COMP);

    const uint32_t *wg_size = params->workgroup_size;
    ngli_bstr_printf(b, "layout(local_size_x=%u, local_size_y=%u, local_size_z=%u) in;\n", NGLI_ARG_VEC3(wg_size));

    int ret;
    if ((ret = inject_uniforms(s, b, params, NGPU_PROGRAM_SHADER_COMP)) < 0 ||
        (ret = inject_textures(s, params, NGPU_PROGRAM_SHADER_COMP)) < 0 ||
        (ret = inject_blocks(s, b, params, NGPU_PROGRAM_SHADER_COMP)) < 0 ||
        (ret = inject_ublock(s, b, NGPU_PROGRAM_SHADER_COMP)) < 0)
        return ret;

    ngli_bstr_print(b, params->comp_base);
    return samplers_preproc(s, params, b);
}

NGLI_STATIC_ASSERT(resource_name_offset, offsetof(struct ngpu_bindgroup_layout_entry, id) == 0);

static int32_t get_ublock_index(const struct pgcraft *s, const char *name, int stage)
{
    const struct pgcraft_compat_info *compat_info = &s->compat_info;
    const struct darray *fields_array = &compat_info->ublocks[stage].fields;
    const struct ngpu_block_field *fields = ngli_darray_data(fields_array);
    for (int32_t i = 0; i < (int32_t)ngli_darray_count(fields_array); i++)
        if (!strcmp(fields[i].name, name))
            return stage << 16 | i;
    return -1;
}

static int32_t get_texture_index(const struct pgcraft *s, const char *name)
{
    const struct ngpu_bindgroup_layout_entry *entries = ngli_darray_data(&s->pipeline_info.desc.textures);
    for (int32_t i = 0; i < (int32_t)ngli_darray_count(&s->pipeline_info.desc.textures); i++) {
        const struct ngpu_bindgroup_layout_entry *entry = &entries[i];
        const char *texture_name = ngli_pgcraft_get_symbol_name(s, entry->id);
        if (!strcmp(texture_name, name))
            return i;
    }
    return -1;
}

static void probe_texture_info_elems(const struct pgcraft *s,
                                     const struct pgcraft_texture *texture,
                                     struct pgcraft_texture_info_field *fields)
{
    for (size_t i = 0; i < NGLI_INFO_FIELD_NB; i++) {
        struct pgcraft_texture_info_field *field = &fields[i];
        char name[MAX_ID_LEN];
        int len = snprintf(name, sizeof(name), "%s%s", texture->name, texture_info_suffixes[i]);
        ngli_assert(len < sizeof(name));
        if (field->type == NGPU_TYPE_NONE)
            field->index = -1;
        else if (is_sampler(field->type) || is_image(field->type))
            field->index = get_texture_index(s, name);
        else
            field->index = ngli_pgcraft_get_uniform_index(s, name, field->stage);
    }
}

static void probe_texture_infos(struct pgcraft *s)
{
    const struct pgcraft_texture *textures = ngli_darray_data(&s->textures);
    struct pgcraft_texture_info *texture_infos = ngli_darray_data(&s->texture_infos);
    for (size_t i = 0; i < ngli_darray_count(&s->texture_infos); i++) {
        const struct pgcraft_texture *texture = &textures[i];
        struct pgcraft_texture_info *info = &texture_infos[i];
        probe_texture_info_elems(s, texture, info->fields);
    }
}

static void probe_ublocks(struct pgcraft *s)
{
    const struct darray *array = &s->pipeline_info.desc.buffers;

    struct pgcraft_compat_info *info = &s->compat_info;
    for (size_t i = 0; i < NGPU_PROGRAM_SHADER_NB; i++) {
        const struct ngpu_block_desc *block = &info->ublocks[i];
        const int32_t binding = info->ubindings[i];

        const size_t block_size = ngpu_block_desc_get_size(block, 0);
        if (!block_size)
            continue;

        const struct ngpu_bindgroup_layout_entry *entries = ngli_darray_data(array);
        for (size_t j = 0; j < ngli_darray_count(array); j++) {
            const struct ngpu_bindgroup_layout_entry *entry = &entries[j];
            if (entry->type == NGPU_TYPE_UNIFORM_BUFFER &&
                entry->binding == binding &&
                entry->stage_flags == (1U << i)) {
                info->uindices[i] = (int32_t)j;
                break;
            }
        }
    }
}

static int probe_pipeline_elems(struct pgcraft *s)
{
    probe_texture_infos(s);
    probe_ublocks(s);

    return 0;
}

#if defined(BACKEND_GL) || defined(BACKEND_GLES)

#define IS_GLSL_ES_MIN(min) (config->backend == NGL_BACKEND_OPENGLES && s->glsl_version >= (min))
#define IS_GLSL_MIN(min)    (config->backend == NGL_BACKEND_OPENGL   && s->glsl_version >= (min))

static void setup_glsl_info_gl(struct pgcraft *s)
{
    struct ngpu_ctx *gpu_ctx = s->gpu_ctx;
    const struct ngl_config *config = &gpu_ctx->config;
    const struct ngpu_ctx_gl *gpu_ctx_gl = (struct ngpu_ctx_gl *)gpu_ctx;
    const struct glcontext *gl = gpu_ctx_gl->glcontext;

    s->sym_vertex_index   = "gl_VertexID";
    s->sym_instance_index = "gl_InstanceID";

    s->glsl_version = gpu_ctx->language_version;

    if (config->backend == NGL_BACKEND_OPENGLES)
        s->glsl_version_suffix = " es";

    s->has_in_out_layout_qualifiers = IS_GLSL_ES_MIN(310) || IS_GLSL_MIN(410);
    s->has_precision_qualifiers     = IS_GLSL_ES_MIN(100);

    s->has_explicit_bindings = IS_GLSL_ES_MIN(310) || IS_GLSL_MIN(420) ||
                               (gl->features & NGLI_FEATURE_GL_SHADING_LANGUAGE_420PACK);

    /*
     * Bindings are shared across all stages. UBO, SSBO, texture and image
     * bindings use distinct binding points.
     */
    s->next_bindings[NGLI_BINDING_TYPE_UBO]     = &s->bindings[NGLI_BINDING_TYPE_UBO];
    s->next_bindings[NGLI_BINDING_TYPE_SSBO]    = &s->bindings[NGLI_BINDING_TYPE_SSBO];
    s->next_bindings[NGLI_BINDING_TYPE_TEXTURE] = &s->bindings[NGLI_BINDING_TYPE_TEXTURE];
    s->next_bindings[NGLI_BINDING_TYPE_IMAGE]   = &s->bindings[NGLI_BINDING_TYPE_IMAGE];
}
#endif

#if defined(BACKEND_VK)
static void setup_glsl_info_vk(struct pgcraft *s)
{
    struct ngpu_ctx *gpu_ctx = s->gpu_ctx;

    s->glsl_version = gpu_ctx->language_version;

    s->sym_vertex_index   = "gl_VertexIndex";
    s->sym_instance_index = "gl_InstanceIndex";

    s->has_explicit_bindings        = 1;
    s->has_in_out_layout_qualifiers = 1;
    s->has_precision_qualifiers     = 0;

    /* Bindings are shared across stages and types */
    for (size_t i = 0; i < NGLI_BINDING_TYPE_NB; i++)
        s->next_bindings[i] = &s->bindings[0];
}
#endif

static void setup_glsl_info(struct pgcraft *s)
{
    struct ngpu_ctx *gpu_ctx = s->gpu_ctx;
    ngli_unused const struct ngl_config *config = &gpu_ctx->config;

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

struct pgcraft *ngli_pgcraft_create(struct ngpu_ctx *gpu_ctx)
{
    struct pgcraft *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;

    s->gpu_ctx = gpu_ctx;

    setup_glsl_info(s);

    ngli_darray_init(&s->texture_infos, sizeof(struct pgcraft_texture_info), 0);
    ngli_darray_init(&s->images, sizeof(struct image *), 0);

    struct pgcraft_compat_info *compat_info = &s->compat_info;
    for (size_t i = 0; i < NGLI_ARRAY_NB(compat_info->ublocks); i++) {
        ngpu_block_desc_init(gpu_ctx, &compat_info->ublocks[i], NGPU_BLOCK_LAYOUT_STD140);
        compat_info->ubindings[i] = -1;
        compat_info->uindices[i] = -1;
    }

    ngli_darray_init(&s->symbols, sizeof(char[MAX_ID_LEN]), 0);

    ngli_darray_init(&s->pipeline_info.desc.textures, sizeof(struct ngpu_bindgroup_layout_entry), 0);
    ngli_darray_init(&s->pipeline_info.desc.buffers, sizeof(struct ngpu_bindgroup_layout_entry), 0);
    ngli_darray_init(&s->pipeline_info.desc.vertex_buffers, sizeof(struct ngpu_vertex_buffer_layout), 0);

    ngli_darray_init(&s->pipeline_info.data.textures, sizeof(struct ngpu_texture_binding), 0);
    ngli_darray_init(&s->pipeline_info.data.buffers, sizeof(struct ngpu_buffer_binding), 0);
    ngli_darray_init(&s->pipeline_info.data.vertex_buffers, sizeof(struct ngpu_buffer *),  0);

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

    if ((ret = alloc_shader(s, NGPU_PROGRAM_SHADER_COMP)) < 0 ||
        (ret = prepare_texture_infos(s, params, 0)) < 0 ||
        (ret = craft_comp(s, params)) < 0)
        return ret;

    const struct ngpu_program_params program_params = {
        .label   = params->program_label,
        .compute = ngli_bstr_strptr(s->shaders[NGPU_PROGRAM_SHADER_COMP]),
    };
    ret = ngpu_pgcache_get_compute_program(&s->gpu_ctx->program_cache, &s->program, &program_params);
    ngli_bstr_freep(&s->shaders[NGPU_PROGRAM_SHADER_COMP]);
    return ret;
}

static int get_program_graphics(struct pgcraft *s, const struct pgcraft_params *params)
{
    int ret;

    ngli_darray_init(&s->vert_out_vars, sizeof(struct pgcraft_iovar), 0);
    for (size_t i = 0; i < params->nb_vert_out_vars; i++) {
        struct pgcraft_iovar *iovar = ngli_darray_push(&s->vert_out_vars, &params->vert_out_vars[i]);
        if (!iovar)
            return NGL_ERROR_MEMORY;
    }

    if ((ret = alloc_shader(s, NGPU_PROGRAM_SHADER_VERT)) < 0 ||
        (ret = alloc_shader(s, NGPU_PROGRAM_SHADER_FRAG)) < 0 ||
        (ret = prepare_texture_infos(s, params, 1)) < 0 ||
        (ret = craft_vert(s, params)) < 0 ||
        (ret = craft_frag(s, params)) < 0)
        return ret;

    const struct ngpu_program_params program_params = {
        .label    = params->program_label,
        .vertex   = ngli_bstr_strptr(s->shaders[NGPU_PROGRAM_SHADER_VERT]),
        .fragment = ngli_bstr_strptr(s->shaders[NGPU_PROGRAM_SHADER_FRAG]),
    };
    ret = ngpu_pgcache_get_graphics_program(&s->gpu_ctx->program_cache, &s->program, &program_params);
    ngli_bstr_freep(&s->shaders[NGPU_PROGRAM_SHADER_VERT]);
    ngli_bstr_freep(&s->shaders[NGPU_PROGRAM_SHADER_FRAG]);
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

    s->compat_info.texture_infos    = ngli_darray_data(&s->texture_infos);
    s->compat_info.images           = ngli_darray_data(&s->images);
    s->compat_info.nb_texture_infos = ngli_darray_count(&s->texture_infos);

#if defined(BACKEND_GL) || defined(BACKEND_GLES)
    struct ngpu_ctx *gpu_ctx = s->gpu_ctx;
    struct ngl_config *config = &gpu_ctx->config;
    if (config->backend == NGL_BACKEND_OPENGL ||
        config->backend == NGL_BACKEND_OPENGLES) {
        if (!s->has_explicit_bindings) {
            /* Force locations and bindings for contexts that do not support
             * explicit locations and bindings */
            ret = ngpu_program_gl_set_locations_and_bindings(s->program, s);
            if (ret < 0)
                return ret;
        }
    }
#endif

    return 0;
}

int32_t ngli_pgcraft_get_uniform_index(const struct pgcraft *s, const char *name, int stage)
{
    return get_ublock_index(s, name, stage);
}

int32_t ngli_pgcraft_get_block_index(const struct pgcraft *s, const char *name, int stage)
{
    const struct darray *array = &s->pipeline_info.desc.buffers;
    const struct ngpu_bindgroup_layout_entry *entries = ngli_darray_data(array);
    for (int32_t i = 0; i < (int32_t)ngli_darray_count(array); i++) {
        const struct ngpu_bindgroup_layout_entry *entry = &entries[i];
        const char *desc_name = ngli_pgcraft_get_symbol_name(s, entry->id);
        if (!strcmp(desc_name, name) && entry->stage_flags == (1U << stage))
            return i;
    }
    return -1;
}

int32_t ngli_pgcraft_get_image_index(const struct pgcraft *s, const char *name)
{
    const struct darray *array = &s->texture_infos;
    const struct pgcraft_texture_info *infos = ngli_darray_data(array);
    for (int32_t i = 0; i < (int32_t)ngli_darray_count(array); i++) {
        const struct pgcraft_texture_info *info = &infos[i];
        const char *desc_name = ngli_pgcraft_get_symbol_name(s, info->id);
        if (!strcmp(desc_name, name))
            return i;
    }
    return -1;
}

const struct pgcraft_compat_info *ngli_pgcraft_get_compat_info(const struct pgcraft *s)
{
    return &s->compat_info;
}

struct ngpu_program *ngli_pgcraft_get_program(const struct pgcraft *s)
{
    return s->program;
}

struct ngpu_vertex_state ngli_pgcraft_get_vertex_state(const struct pgcraft *s)
{
    return (const struct ngpu_vertex_state) {
        .buffers = ngli_darray_data(&s->pipeline_info.desc.vertex_buffers),
        .nb_buffers = ngli_darray_count(&s->pipeline_info.desc.vertex_buffers),
    };
}

struct ngpu_vertex_resources ngli_pgcraft_get_vertex_resources(const struct pgcraft *s)
{
    const struct ngpu_vertex_resources resources = {
        .vertex_buffers    = ngli_darray_data(&s->pipeline_info.data.vertex_buffers),
        .nb_vertex_buffers = ngli_darray_count(&s->pipeline_info.data.vertex_buffers),
    };
    return resources;
}

int32_t ngli_pgcraft_get_vertex_buffer_index(const struct pgcraft *s, const char *name)
{
    const struct darray *array = &s->pipeline_info.desc.vertex_buffers;
    struct ngpu_vertex_buffer_layout *layouts = ngli_darray_data(array);
    for (int32_t i = 0; i < (int32_t)ngli_darray_count(array); i++) {
        struct ngpu_vertex_buffer_layout *layout = &layouts[i];
        for (size_t j = 0; j < layout->nb_attributes; j++) {
            struct ngpu_vertex_attribute *attribute = &layout->attributes[j];
            const char *attribute_name = ngli_pgcraft_get_symbol_name(s, attribute->id);
            if (!strcmp(attribute_name, name))
                return i;
        }
    }
    return -1;
}

const char *ngli_pgcraft_get_symbol_name(const struct pgcraft *s, size_t id)
{
    return ngli_darray_get(&s->symbols, id);
}

struct ngpu_bindgroup_layout_desc ngli_pgcraft_get_bindgroup_layout_desc(const struct pgcraft *s)
{
    const struct ngpu_bindgroup_layout_desc bindgroup_layout_params = {
        .textures    = ngli_darray_data(&s->pipeline_info.desc.textures),
        .nb_textures = ngli_darray_count(&s->pipeline_info.desc.textures),
        .buffers     = ngli_darray_data(&s->pipeline_info.desc.buffers),
        .nb_buffers  = ngli_darray_count(&s->pipeline_info.desc.buffers),
    };
    return bindgroup_layout_params;
}

struct ngpu_bindgroup_resources ngli_pgcraft_get_bindgroup_resources(const struct pgcraft *s)
{
    const struct ngpu_bindgroup_resources resources = {
        .textures          = ngli_darray_data(&s->pipeline_info.data.textures),
        .nb_textures       = ngli_darray_count(&s->pipeline_info.data.textures),
        .buffers           = ngli_darray_data(&s->pipeline_info.data.buffers),
        .nb_buffers        = ngli_darray_count(&s->pipeline_info.data.buffers),
    };
    return resources;
}

void ngli_pgcraft_freep(struct pgcraft **sp)
{
    struct pgcraft *s = *sp;
    if (!s)
        return;

    ngli_darray_reset(&s->textures);
    ngli_darray_reset(&s->texture_infos);
    ngli_darray_reset(&s->images);
    ngli_darray_reset(&s->vert_out_vars);

    struct pgcraft_compat_info *compat_info = &s->compat_info;
    for (size_t i = 0; i < NGLI_ARRAY_NB(compat_info->ublocks); i++) {
        ngpu_block_desc_reset(&compat_info->ublocks[i]);
    }

    for (size_t i = 0; i < NGLI_ARRAY_NB(s->shaders); i++)
        ngli_bstr_freep(&s->shaders[i]);

    ngli_darray_reset(&s->symbols);

    ngli_darray_reset(&s->pipeline_info.desc.textures);
    ngli_darray_reset(&s->pipeline_info.desc.buffers);
    ngli_darray_reset(&s->pipeline_info.desc.vertex_buffers);

    ngli_darray_reset(&s->pipeline_info.data.textures);
    ngli_darray_reset(&s->pipeline_info.data.buffers);
    ngli_darray_reset(&s->pipeline_info.data.vertex_buffers);

    ngli_freep(sp);
}
