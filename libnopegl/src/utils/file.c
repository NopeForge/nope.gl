/*
 * Copyright 2023-2025 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#endif

#include <stdint.h>
#include <string.h>

#include "log.h"
#include "file.h"
#include "utils.h"

int ngli_get_filesize(const char *filename, int64_t *size)
{
#ifdef _WIN32
    HANDLE file_handle = CreateFile(TEXT(filename), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file_handle == INVALID_HANDLE_VALUE)
        return NGL_ERROR_IO;

    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(file_handle, &file_size)) {
        CloseHandle(file_handle);
        return NGL_ERROR_IO;
    }
    *size = file_size.QuadPart;
    CloseHandle(file_handle);
#else
    struct stat st;
    int ret = stat(filename, &st);
    if (ret == -1) {
        LOG(ERROR, "could not stat '%s': %s", filename, strerror(errno));
        return NGL_ERROR_IO;
    }
    *size = st.st_size;
#endif
    return 0;
}
