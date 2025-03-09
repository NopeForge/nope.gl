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

#ifndef REFCOUNT_H
#define REFCOUNT_H

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>

typedef void (*ngli_freep_func)(void **sp);

struct ngli_rc {
    size_t count;
    ngli_freep_func freep;
};

#define NGLI_RC_CHECK_STRUCT(name) static_assert(offsetof(struct name, rc) == 0, #name "_rc")
#define NGLI_RC_CREATE(fn) (struct ngli_rc) { .count=1, .freep=(ngli_freep_func)fn }
#define NGLI_RC_REF(s) (void *)ngli_rc_ref((struct ngli_rc *)(s))
#define NGLI_RC_UNREFP(sp) ngli_rc_unrefp((struct ngli_rc **)(sp))

struct ngli_rc *ngli_rc_ref(struct ngli_rc *s);
void ngli_rc_unrefp(struct ngli_rc **sp);

#endif /* REFCOUNT_H */
