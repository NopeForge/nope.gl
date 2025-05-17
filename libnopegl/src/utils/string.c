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

#include "bstr.h"
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
    char *p = NULL;
    va_list va;
    int len;

    va_start(va, fmt);
    len = vsnprintf(NULL, 0, fmt, va);
    va_end(va);
    if (len < 0)
        goto end;

    size_t n = (size_t)len + 1;
    p = ngli_malloc(n);
    if (!p)
        goto end;

    va_start(va, fmt);
    len = vsnprintf(p, n, fmt, va);
    va_end(va);
    if (len < 0)
        ngli_freep(&p);

end:
    return p;
}
static int count_lines(const char *s)
{
    int count = 0;
    while (*s) {
        const size_t len = strcspn(s, "\n");
        count++;
        if (!s[len])
            break;
        s += len + 1;
    }
    return count;
}

static int count_digits(int x)
{
    int n = 1;
    while (x /= 10)
        n++;
    return n;
}

char *ngli_numbered_lines(const char *s)
{
    struct bstr *b = ngli_bstr_create();
    if (!b)
        return NULL;

    const int nb_lines = count_lines(s);
    const int nb_digits = count_digits(nb_lines);
    int line = 1;
    while (*s) {
        const size_t len = strcspn(s, "\n");
        ngli_bstr_printf(b, "%*d %.*s\n", nb_digits, line++, (int)len, s);
        if (!s[len])
            break;
        s += len + 1;
    }

    char *ret = ngli_bstr_strdup(b);
    ngli_bstr_freep(&b);
    if (!ret)
        return NULL;

    size_t len = strlen(ret);
    while (len && ret[len - 1] == '\n')
        ret[--len] = 0;

    return ret;
}
