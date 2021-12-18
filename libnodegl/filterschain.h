/*
 * Copyright 2021 GoPro Inc.
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

#ifndef FILTERSCHAIN_H
#define FILTERSCHAIN_H

#include "internal.h"

struct filterschain;

/* We can not use the #include mechanism from glsl2c.py because a helper can
 * appear in several filters */
#define NGLI_FILTER_HELPER_MISC_UTILS  (1 << 0)
#define NGLI_FILTER_HELPER_SRGB2LINEAR (1 << 1)
#define NGLI_FILTER_HELPER_LINEAR2SRGB (1 << 2)

struct filter {
    const char *name;
    const char *code;
    struct darray resources; /* struct pgcraft_uniform */
    uint32_t helpers;
};

struct filterschain *ngli_filterschain_create(void);
int ngli_filterschain_init(struct filterschain *s, const char *source_name, const char *source_code, uint32_t helpers);
int ngli_filterschain_add_filter(struct filterschain *s, const struct filter *filter);
char *ngli_filterschain_get_combination(struct filterschain *s);
const struct darray *ngli_filterschain_get_resources(struct filterschain *s);
void ngli_filterschain_freep(struct filterschain **sp);

#endif
