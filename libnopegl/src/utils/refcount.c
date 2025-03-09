/*
 * Copyright 2023-2025 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#include "refcount.h"

struct ngli_rc *ngli_rc_ref(struct ngli_rc *s)
{
    s->count++;
    return s;
}

void ngli_rc_unrefp(struct ngli_rc **sp)
{
    struct ngli_rc *s = *sp;
    if (!s)
        return;

    if (s->count-- == 1)
        s->freep((void **)sp);

    *sp = NULL;
}
