/*
 * Copyright 2024-2025 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "string.h"

char *ngli_strdup(const char *s)
{
    if (!s)
        return NULL;
    const size_t len = strlen(s);
    char *r = ngli_malloc(len + 1);
    if (!r)
        return NULL;
    memcpy(r, s, len);
    r[len] = 0;
    return r;
}

char *ngli_asprintf(const char *fmt, ...)
{
    va_list va;

    va_start(va, fmt);
    int len = vsnprintf(NULL, 0, fmt, va);
    va_end(va);
    if (len < 0)
        return NULL;

    const size_t n = (size_t)len + 1;
    char *p = ngli_malloc(n);
    if (!p)
        return NULL;

    va_start(va, fmt);
    len = vsnprintf(p, n, fmt, va);
    va_end(va);
    if (len < 0)
        ngli_freep(&p);

    return p;
}
