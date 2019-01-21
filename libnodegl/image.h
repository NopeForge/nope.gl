/*
 * Copyright 2018 GoPro Inc.
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

#ifndef IMAGE_H
#define IMAGE_H

#include "texture.h"
#include "utils.h"

enum image_layout {
    NGLI_IMAGE_LAYOUT_NONE,
    NGLI_IMAGE_LAYOUT_DEFAULT,
    NGLI_IMAGE_LAYOUT_NV12,
    NGLI_IMAGE_LAYOUT_NV12_RECTANGLE,
    NGLI_IMAGE_LAYOUT_MEDIACODEC,
    NGLI_NB_IMAGE_LAYOUTS
};

struct image {
    enum image_layout layout;
    const struct texture *planes[4];
    int nb_planes;
    NGLI_ALIGNED_MAT(coordinates_matrix);
    double ts;
};

void ngli_image_init(struct image *s, enum image_layout layout, ...);
void ngli_image_reset(struct image *s);
uint64_t ngli_image_get_memory_size(const struct image *s);

#endif
