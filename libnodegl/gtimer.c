/*
 * Copyright 2020 GoPro Inc.
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

#include "gctx.h"
#include "gtimer.h"

struct gtimer *ngli_gtimer_create(struct gctx *gctx)
{
    return gctx->class->gtimer_create(gctx);
}

int ngli_gtimer_init(struct gtimer *s)
{
    return s->gctx->class->gtimer_init(s);
}

int ngli_gtimer_start(struct gtimer *s)
{
    return s->gctx->class->gtimer_start(s);
}

int ngli_gtimer_stop(struct gtimer *s)
{
    return s->gctx->class->gtimer_stop(s);
}

int64_t ngli_gtimer_read(struct gtimer *s)
{
    return s->gctx->class->gtimer_read(s);
}

void ngli_gtimer_freep(struct gtimer **sp)
{
    if (!*sp)
        return;
    (*sp)->gctx->class->gtimer_freep(sp);
}
