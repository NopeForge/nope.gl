/*
 * Copyright 2021-2022 GoPro Inc.
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

#ifndef BLENDING_H
#define BLENDING_H

#include "ngpu/graphics_state.h"

enum ngli_blending {
    NGLI_BLENDING_DEFAULT,
    NGLI_BLENDING_SRC_OVER,
    NGLI_BLENDING_DST_OVER,
    NGLI_BLENDING_SRC_OUT,
    NGLI_BLENDING_DST_OUT,
    NGLI_BLENDING_SRC_IN,
    NGLI_BLENDING_DST_IN,
    NGLI_BLENDING_SRC_ATOP,
    NGLI_BLENDING_DST_ATOP,
    NGLI_BLENDING_XOR,
    NGLI_BLENDING_MAX_ENUM = 0x7FFFFFFF
};

extern const struct param_choices ngli_blending_choices;
int ngli_blending_apply_preset(struct ngpu_graphics_state *state, enum ngli_blending preset);

#endif
