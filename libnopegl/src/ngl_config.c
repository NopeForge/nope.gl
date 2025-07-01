/*
 * Copyright 2024 Matthieu Bouron <matthieu.bouron@gmail.com>
 * Copyright 2016-2022 GoPro Inc.
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

#include "config.h"
#include "log.h"
#include "ngl_config.h"
#include "utils/memory.h"
#include "utils/string.h"

int ngli_config_set_debug_defaults(struct ngl_config *config)
{
    switch (config->backend) {
        case NGL_BACKEND_OPENGL:
        case NGL_BACKEND_OPENGLES:
            config->debug |= DEBUG_GL;
            break;
        case NGL_BACKEND_VULKAN:
            config->debug |= DEBUG_VK;
            break;
        default:
            return NGL_ERROR_UNSUPPORTED;
    }
    return config->debug;
}

int ngli_config_copy(struct ngl_config *dst, const struct ngl_config *src)
{
    struct ngl_config tmp = *src;

    if (src->hud_export_filename) {
        tmp.hud_export_filename = ngli_strdup(src->hud_export_filename);
        if (!tmp.hud_export_filename)
            return NGL_ERROR_MEMORY;
    }

    if (src->backend_config) {
        if (src->backend == NGL_BACKEND_OPENGL ||
            src->backend == NGL_BACKEND_OPENGLES) {
            const size_t size = sizeof(struct ngl_config_gl);
            tmp.backend_config = ngli_memdup(src->backend_config, size);
            if (!tmp.backend_config) {
                ngli_freep(&tmp.hud_export_filename);
                return NGL_ERROR_MEMORY;
            }
        } else {
            ngli_freep(&tmp.hud_export_filename);
            LOG(ERROR, "backend_config %p is not supported by backend %u",
                src->backend_config, src->backend);
            return NGL_ERROR_UNSUPPORTED;
        }
    }

    *dst = tmp;

    return 0;
}

void ngli_config_reset(struct ngl_config *config)
{
    ngli_freep(&config->backend_config);
    ngli_freep(&config->hud_export_filename);
    memset(config, 0, sizeof(*config));
}
