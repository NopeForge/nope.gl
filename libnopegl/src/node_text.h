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

#ifndef NODE_TEXT_H
#define NODE_TEXT_H

#include <stdint.h>

enum {
    NGLI_TEXT_EFFECT_CHAR,
    NGLI_TEXT_EFFECT_CHAR_NOSPACE,
    NGLI_TEXT_EFFECT_WORD,
    NGLI_TEXT_EFFECT_LINE,
    NGLI_TEXT_EFFECT_TEXT,
};

enum {
    NGLI_TEXT_ANCHOR_REF_CHAR,
    NGLI_TEXT_ANCHOR_REF_BOX,
    NGLI_TEXT_ANCHOR_REF_VIEWPORT,
};

struct fontface_opts {
    char *path;
    int32_t index;
};

#endif
