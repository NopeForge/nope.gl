/*
 * Copyright 2017 GoPro Inc.
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

#include <OpenGLES/EAGL.h>

#include "glcontext.h"
#include "nodegl.h"

struct glcontext_eagl {
    EAGLContext *handle;
    CFBundleRef framework;
};

static int glcontext_eagl_init(struct glcontext *glcontext, void *display, void *window, void *handle)
{
    struct glcontext_eagl *glcontext_eagl = glcontext->priv_data;

    glcontext_eagl->handle = handle ? (EAGLContext *)handle : [EAGLContext currentContext];

    glcontext_eagl->framework = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.opengles"));
    if (!glcontext_eagl->framework)
        return -1;

    return 0;
}

static void glcontext_eagl_uninit(struct glcontext *glcontext)
{
    struct glcontext_eagl *glcontext_eagl = glcontext->priv_data;

    if (glcontext_eagl->framework)
        CFRelease(glcontext_eagl->framework);
}

static int glcontext_eagl_create(struct glcontext *glcontext, struct glcontext *other)
{
    return 0;
}

static int glcontext_eagl_make_current(struct glcontext *glcontext, int current)
{
    int ret;
    struct glcontext_eagl *glcontext_eagl = glcontext->priv_data;

    if (current) {
        ret = [EAGLContext setCurrentContext:glcontext_eagl->handle];
    } else {
        ret = [EAGLContext setCurrentContext:nil];
    }

    return ret;
}

static void *glcontext_eagl_get_display(struct glcontext *glcontext)
{
    struct glcontext_eagl *glcontext_eagl = glcontext->priv_data;
    return NULL;
}

static void *glcontext_eagl_get_window(struct glcontext *glcontext)
{
    struct glcontext_eagl *glcontext_eagl = glcontext->priv_data;
    return NULL;
}

static void *glcontext_eagl_get_handle(struct glcontext *glcontext)
{
    struct glcontext_eagl *glcontext_eagl = glcontext->priv_data;
    return glcontext_eagl->handle;
}

static void *glcontext_eagl_get_proc_address(struct glcontext *glcontext, const char *name)
{
    struct glcontext_eagl *glcontext_eagl = glcontext->priv_data;

    CFStringRef symbol_name = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingASCII);
    if (!symbol_name) {
        return NULL;
    }

    void *symbol_address = CFBundleGetFunctionPointerForName(glcontext_eagl->framework, symbol_name);
    CFRelease(symbol_name);

    return symbol_address;
}

const struct glcontext_class ngli_glcontext_eagl_class = {
    .init = glcontext_eagl_init,
    .uninit = glcontext_eagl_uninit,
    .create = glcontext_eagl_create,
    .make_current = glcontext_eagl_make_current,
    .get_display = glcontext_eagl_get_display,
    .get_window = glcontext_eagl_get_window,
    .get_handle = glcontext_eagl_get_handle,
    .get_proc_address = glcontext_eagl_get_proc_address,
    .priv_size = sizeof(struct glcontext_eagl),
};
