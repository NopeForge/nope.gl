/*
 * Copyright 2022 GoPro Inc.
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

#include "internal.h"

const struct api_impl api_vk = {
    .configure          = ngli_ctx_configure,
    .resize             = ngli_ctx_resize,
    .set_capture_buffer = ngli_ctx_set_capture_buffer,
    .set_scene          = ngli_ctx_set_scene,
    .prepare_draw       = ngli_ctx_prepare_draw,
    .draw               = ngli_ctx_draw,
    .reset              = ngli_ctx_reset,
};
