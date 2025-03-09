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

#include <stdint.h>

#ifdef _WIN32
#define POW10_9 1000000000
#include <Windows.h>
#else
#include <time.h>
#endif

#include "time.h"

int64_t ngli_gettime_relative(void)
{
#ifdef _WIN32
    // reference: https://github.com/mirror/mingw-w64/blob/master/mingw-w64-libraries/winpthreads/src/clock.c
    LARGE_INTEGER pf, pc;
    QueryPerformanceFrequency(&pf);
    QueryPerformanceCounter(&pc);
    int64_t tv_sec = pc.QuadPart / pf.QuadPart;
    int64_t tv_nsec = (int)(((pc.QuadPart % pf.QuadPart) * POW10_9 + (pf.QuadPart >> 1)) / pf.QuadPart);
    if (tv_nsec >= POW10_9) {
        tv_sec++;
        tv_nsec -= POW10_9;
    }
    return tv_sec * 1000000 + tv_nsec / 1000;
#else
    struct timespec ts;
    int ret = clock_gettime(CLOCK_MONOTONIC, &ts);
    return ret != 0 ? 0 : ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
#endif
}
