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

#define _GNU_SOURCE
#include <pthread.h>

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "log.h"
#include "memory.h"
#include "utils.h"

char *ngli_strdup(const char *s)
{
    char *r = NULL;
    if (s) {
        size_t len = strlen(s);
        r = ngli_malloc(len + 1);
        if (!r)
            return NULL;
        strcpy(r, s);
        r[len] = 0;
    }
    return r;
}

int64_t ngli_gettime(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return 1000000 * (int64_t)tv.tv_sec + tv.tv_usec;
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

    p = ngli_malloc(len + 1);
    if (!p)
        goto end;

    va_start(va, fmt);
    len = vsnprintf(p, len + 1, fmt, va);
    va_end(va);
    if (len < 0) {
        ngli_free(p);
        p = NULL;
    }

end:
    return p;
}

uint32_t ngli_crc32(const char *s)
{
    uint32_t crc = ~0;
    for (int i = 0; s[i]; i++) {
        crc ^= s[i];
        for (int j = 0; j < 8; j++) {
            const uint32_t mask = -(crc & 1);
            crc = (crc >> 1) ^ (mask & 0xEDB88320);
        }
    }
    return ~crc;
}

void ngli_thread_set_name(const char *name)
{
#if defined(__APPLE__)
    pthread_setname_np(name);
#elif ((defined(__linux__) && defined(__GLIBC__)) || defined(__ANDROID__))
    pthread_setname_np(pthread_self(), name);
#endif
}
