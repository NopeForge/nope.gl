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

#ifndef BUFFER_H
#define BUFFER_H

#include "glcontext.h"

struct graphic_buffer {
    GLuint id;
    int size;
    int usage;
};

int ngli_graphic_buffer_allocate(struct glcontext *gl,
                                 struct graphic_buffer *buffer,
                                 int size,
                                 int usage);

int ngli_graphic_buffer_upload(struct glcontext *gl,
                               struct graphic_buffer *buffer,
                               void *data,
                               int size);

void ngli_graphic_buffer_free(struct glcontext *gl,
                              struct graphic_buffer *buffer);

#endif
