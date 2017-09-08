/*
 * Copyright 2017 GoPro Inc.
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

#ifndef NDICT_H
#define NDICT_H

#include "nodegl.h"
#include "utils.h"

struct ndict_entry {
    char *name;
    struct ngl_node *node;
};

struct ndict;

int ngli_ndict_count(struct ndict *ndict);
struct ndict_entry *ngli_ndict_get(struct ndict *ndict, const char *name, struct ndict_entry *prev);
int ngli_ndict_set(struct ndict **ndictp, const char *name, struct ngl_node *node);
void ngli_ndict_freep(struct ndict **ndictp);

#endif
