/*
 * Copyright 2016 GoPro Inc.
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

#include "gl_utils.h"

#include "log.h"

GLenum ngli_check_gl_error(void)
{
    static const char * const errors[] = {
        [GL_INVALID_ENUM]                   = "GL_INVALID_ENUM",
        [GL_INVALID_VALUE]                  = "GL_INVALID_VALUE",
        [GL_INVALID_OPERATION]              = "GL_INVALID_OPERATION",
        [GL_INVALID_FRAMEBUFFER_OPERATION ] = "GL_INVALID_FRAMEBUFFER_OPERATION",
        [GL_OUT_OF_MEMORY]                  = "GL_OUT_OF_MEMORY",
    };
    const GLenum error = glGetError();

    if (!error)
        return error;

    if (error < NGLI_ARRAY_NB(errors))
        LOG(ERROR, "detected gl error: %s", errors[error]);
    else
        LOG(ERROR, "detected gl error: %d", error);

    return error;
}
