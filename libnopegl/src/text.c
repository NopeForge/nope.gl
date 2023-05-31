/*
 * Copyright 2023 Nope Project
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

#include "memory.h"
#include "text.h"

extern const struct text_cls ngli_text_builtin;

struct text *ngli_text_create(struct ngl_ctx *ctx)
{
    struct text *s = ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->ctx = ctx;
    return s;
}

int ngli_text_init(struct text *s, const struct text_config *cfg)
{
    s->config = *cfg;

    ngli_darray_init(&s->chars, sizeof(struct char_info), 0);

    s->cls = &ngli_text_builtin;
    if (s->cls->priv_size) {
        s->priv_data = ngli_calloc(1, s->cls->priv_size);
        if (!s->priv_data)
            return NGL_ERROR_MEMORY;
    }
    return s->cls->init(s);
}

int ngli_text_set_string(struct text *s, const char *str)
{
    struct darray chars_internal_array;
    ngli_darray_init(&chars_internal_array, sizeof(struct char_info_internal), 0);

    int ret = s->cls->set_string(s, str, &chars_internal_array);
    if (ret < 0)
        goto end;

    s->width  = NGLI_I26D6_TO_I32_TRUNCATED(s->width);
    s->height = NGLI_I26D6_TO_I32_TRUNCATED(s->height);

    /* Expose characters publicly */
    ngli_darray_clear(&s->chars);
    struct char_info_internal *chars_internal = ngli_darray_data(&chars_internal_array);
    for (size_t i = 0; i < ngli_darray_count(&chars_internal_array); i++) {
        const struct char_info_internal *chr_internal = &chars_internal[i];

        const struct char_info chr = {
            .x = NGLI_I26D6_TO_F32(chr_internal->x) / (float)s->width,
            .y = NGLI_I26D6_TO_F32(chr_internal->y) / (float)s->height,
            .w = NGLI_I26D6_TO_F32(chr_internal->w) / (float)s->width,
            .h = NGLI_I26D6_TO_F32(chr_internal->h) / (float)s->height,
            .atlas_coords = {
                (float)chr_internal->atlas_coords[0] / (float)s->texture->params.width,
                (float)chr_internal->atlas_coords[1] / (float)s->texture->params.height,
                (float)chr_internal->atlas_coords[2] / (float)s->texture->params.width,
                (float)chr_internal->atlas_coords[3] / (float)s->texture->params.height,
            },
        };

        if (!ngli_darray_push(&s->chars, &chr)) {
            ret = NGL_ERROR_MEMORY;
            goto end;
        }
    }

end:
    ngli_darray_reset(&chars_internal_array);
    return ret;
}

void ngli_text_freep(struct text **sp)
{
    struct text *s = *sp;
    if (s->cls->reset)
        s->cls->reset(s);
    ngli_freep(&s->priv_data);
    ngli_darray_reset(&s->chars);
    ngli_freep(sp);
}
