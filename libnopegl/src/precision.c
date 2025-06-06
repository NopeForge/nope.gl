/*
 * Copyright 2020-2022 GoPro Inc.
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

#include "precision.h"
#include "ngpu/type.h"

const struct param_choices ngli_precision_choices = {
    .name = "precision",
    .consts = {
        {"auto",   NGPU_PRECISION_AUTO,   .desc=NGLI_DOCSTRING("automatic")},
        {"high",   NGPU_PRECISION_HIGH,   .desc=NGLI_DOCSTRING("high")},
        {"medium", NGPU_PRECISION_MEDIUM, .desc=NGLI_DOCSTRING("medium")},
        {"low",    NGPU_PRECISION_LOW,    .desc=NGLI_DOCSTRING("low")},
        {NULL}
    }
};
