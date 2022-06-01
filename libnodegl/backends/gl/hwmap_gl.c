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

#include <stdlib.h>

#include "config.h"

extern const struct hwmap_class ngli_hwmap_mc_gl_class;
extern const struct hwmap_class ngli_hwmap_vaapi_gl_class;
extern const struct hwmap_class ngli_hwmap_vt_darwin_gl_class;
extern const struct hwmap_class ngli_hwmap_vt_ios_gl_class;

const struct hwmap_class *ngli_hwmap_gl_classes[] = {
#if defined(TARGET_ANDROID)
    &ngli_hwmap_mc_gl_class,
#elif defined(HAVE_VAAPI)
    &ngli_hwmap_vaapi_gl_class,
#elif defined(TARGET_DARWIN)
    &ngli_hwmap_vt_darwin_gl_class,
#elif defined(TARGET_IPHONE)
    &ngli_hwmap_vt_ios_gl_class,
#endif
    NULL
};
