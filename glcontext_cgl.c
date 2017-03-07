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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <OpenGL/CGLCurrent.h>
#include <CoreFoundation/CFBundle.h>

#include "glcontext.h"
#include "nodegl.h"

struct glcontext_cgl {
    CGLContextObj handle;
    CFBundleRef framework;
};

static int glcontext_cgl_init(struct glcontext *glcontext, void *display, void *window, void *handle)
{
    struct glcontext_cgl *glcontext_cgl = glcontext->priv_data;

    glcontext_cgl->handle  = handle  ? *(CGLContextObj *)handle : CGLGetCurrentContext();

    if (!glcontext_cgl->handle)
        return -1;

    CFBundleRef framework = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.opengl"));
    if (!framework)
        return -1;

    glcontext_cgl->framework = (CFBundleRef)CFRetain(framework);
    if (!glcontext_cgl->framework)
        return -1;

    return 0;
}

static int glcontext_cgl_make_current(struct glcontext *glcontext, int current)
{
    CGLError error;
    struct glcontext_cgl *glcontext_cgl = glcontext->priv_data;

    if (current) {
        error = CGLSetCurrentContext(glcontext_cgl->handle);
    } else {
        error = CGLSetCurrentContext(NULL);
    }

    return error == kCGLNoError ? 0 : -1;
}

static void *glcontext_cgl_get_handle(struct glcontext *glcontext)
{
    struct glcontext_cgl *glcontext_cgl = glcontext->priv_data;
    return glcontext_cgl->handle;
}

static void *glcontext_cgl_get_proc_address(struct glcontext *glcontext, const char *name)
{
    struct glcontext_cgl *glcontext_cgl = glcontext->priv_data;

    CFStringRef symbol_name = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingASCII);
    if (!symbol_name) {
        return NULL;
    }

    void *symbol_address = CFBundleGetFunctionPointerForName(glcontext_cgl->framework, symbol_name);
    CFRelease(symbol_name);

    return symbol_address;
}

static void glcontext_cgl_uninit(struct glcontext *glcontext)
{
    struct glcontext_cgl *glcontext_cgl = glcontext->priv_data;

    if (glcontext_cgl->framework)
        CFRelease(glcontext_cgl->framework);
}

const struct glcontext_class ngli_glcontext_cgl_class = {
    .init = glcontext_cgl_init,
    .uninit = glcontext_cgl_uninit,
    .make_current = glcontext_cgl_make_current,
    .get_handle = glcontext_cgl_get_handle,
    .get_proc_address = glcontext_cgl_get_proc_address,
    .priv_size = sizeof(struct glcontext_cgl),
};
