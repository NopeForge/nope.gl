/*
 * Copyright 2024 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#ifndef BUFFER_LAYOUT_H
#define BUFFER_LAYOUT_H

#include <stdint.h>
#include <stddef.h>

#include "type.h"
#include "gpu_format.h"

/*
 * Helper structure to specify the content (or a slice) of a buffer
 */
struct buffer_layout {
    int type;       // any of NGLI_TYPE_*
    int format;     // any of NGLI_FORMAT_*
    size_t stride;  // stride of 1 element, in bytes
    size_t comp;    // number of components per element
    size_t count;   // number of elements
    size_t offset;  // offset where the data starts in the buffer, in bytes
};

#endif
