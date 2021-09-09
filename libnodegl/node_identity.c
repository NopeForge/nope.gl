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

#include <string.h>

#include "darray.h"
#include "nodegl.h"
#include "internal.h"
#include "math_utils.h"

static int identity_init(struct ngl_node *node)
{
    struct transform_priv *s = node->priv_data;
    static const NGLI_ALIGNED_MAT(id_matrix) = NGLI_MAT4_IDENTITY;
    memcpy(s->matrix, id_matrix, sizeof(s->matrix));
    return 0;
}

const struct node_class ngli_identity_class = {
    .id        = NGL_NODE_IDENTITY,
    .name      = "Identity",
    .init      = identity_init,
    .priv_size = sizeof(struct transform_priv),
    .file      = __FILE__,
};
