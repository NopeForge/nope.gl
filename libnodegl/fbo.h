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

#ifndef FBO_H
#define FBO_H

#include "darray.h"
#include "glcontext.h"
#include "texture.h"

struct fbo_params {
    int width;
    int height;
    int nb_attachments;
    const struct texture **attachments;
};

struct fbo {
    struct glcontext *gl;
    int width;
    int height;
    int nb_color_attachments;

    GLuint id;
    GLuint prev_id;
    struct darray depth_indices;
};

int ngli_fbo_init(struct fbo *fbo, struct glcontext *gl, const struct fbo_params *params);
int ngli_fbo_bind(struct fbo *fbo);
int ngli_fbo_unbind(struct fbo *fbo);
void ngli_fbo_invalidate_depth_buffers(struct fbo *fbo);
void ngli_fbo_blit(struct fbo *fbo, struct fbo *dst, int vflip);
void ngli_fbo_read_pixels(struct fbo *fbo, uint8_t *data);
void ngli_fbo_reset(struct fbo *fbo);

#endif
