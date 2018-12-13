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

#include "buffer.h"
#include "glcontext.h"
#include "glincludes.h"

int ngli_graphic_buffer_allocate(struct graphic_buffer *buffer,
                                 struct glcontext *gl,
                                 int size,
                                 int usage)
{
    buffer->gl = gl;
    buffer->size = size;
    buffer->usage = usage;
    ngli_glGenBuffers(gl, 1, &buffer->id);
    ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, buffer->id);
    ngli_glBufferData(gl, GL_ARRAY_BUFFER, size, NULL, usage);
    return 0;
}

int ngli_graphic_buffer_upload(struct graphic_buffer *buffer,
                               void *data,
                               int size)
{
    struct glcontext *gl = buffer->gl;
    ngli_glBindBuffer(gl, GL_ARRAY_BUFFER, buffer->id);
    ngli_glBufferSubData(gl, GL_ARRAY_BUFFER, 0, size, data);
    return 0;
}

void ngli_graphic_buffer_free(struct graphic_buffer *buffer)
{
    if (!buffer->gl)
        return;
    ngli_glDeleteBuffers(buffer->gl, 1, &buffer->id);
    memset(buffer, 0, sizeof(*buffer));
}
