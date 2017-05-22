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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "log.h"

static void default_callback(void *arg, int level, const char *filename, int ln,
                             const char *fn, const char *fmt, va_list vl)
{
    char logline[512];
    static const char * const log_strs[] = {
        [NGL_LOG_DEBUG]   = "DEBUG",
        [NGL_LOG_VERBOSE] = "VERBOSE",
        [NGL_LOG_INFO]    = "INFO",
        [NGL_LOG_WARNING] = "WARNING",
        [NGL_LOG_ERROR]   = "ERROR",
    };
    vsnprintf(logline, sizeof(logline), fmt, vl);

    const char *color_start = "", *color_end = "";

#if !defined(TARGET_IPHONE) && !defined(TARGET_ANDROID) && !defined(TARGET_MINGW_W64)
    if (isatty(1) && getenv("TERM")) {
        static const char * const colors[] = {
            [NGL_LOG_DEBUG]   = "\033[0;32m", // green
            [NGL_LOG_VERBOSE] = "\033[1;32m", // green (light)
            [NGL_LOG_INFO]    = "\033[0m",    // no color
            [NGL_LOG_WARNING] = "\033[1;33m", // yellow (light)
            [NGL_LOG_ERROR]   = "\033[0;31m", // red
        };
        color_start = colors[level];
        color_end = "\033[0m";
    }
#endif

    printf("%s[%s] %s:%d %s: %s%s\n", color_start,
           log_strs[level], filename, ln, fn, logline,
           color_end);
}

static struct {
    void *user_arg;
    ngl_log_callback_type callback;
    int min_level;
} log_ctx = {
    .callback  = default_callback,
    .min_level = NGL_LOG_INFO,
};

void ngl_log_set_callback(void *arg, ngl_log_callback_type callback)
{
    log_ctx.user_arg = arg;
    log_ctx.callback = callback;
}

void ngl_log_set_min_level(int level)
{
    log_ctx.min_level = level;
}

void ngli_log_print(int log_level, const char *filename,
                    int ln, const char *fn, const char *fmt, ...)
{
    va_list arg_list;

    if (log_level < log_ctx.min_level)
        return;

    va_start(arg_list, fmt);
    log_ctx.callback(log_ctx.user_arg, log_level, filename, ln, fn, fmt, arg_list);
    va_end(arg_list);
}
