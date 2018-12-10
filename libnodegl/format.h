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

#ifndef FORMAT_H
#define FORMAT_H

#include "formats_register.h"
#include "glcontext.h"
#include "glincludes.h"

#define DECLARE_FORMAT(format, size, name, doc) format,

enum {
    NGLI_FORMATS(DECLARE_FORMAT)
};

int ngli_format_get_gl_format_type(struct glcontext *gl,
                                   int data_format,
                                   GLint *formatp,
                                   GLint *internal_formatp,
                                   GLenum *typep);

#endif
