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

#ifndef HWUPLOAD_MC
#define HWUPLOAD_MC

#include <sxplayer.h>

#include "hwupload.h"
#include "nodes.h"

int ngli_hwupload_mc_get_config_from_frame(struct ngl_node *node,
                                           struct sxplayer_frame *frame,
                                           struct hwupload_config *config);

int ngli_hwupload_mc_init(struct ngl_node *node,
                          struct hwupload_config *config);

int ngli_hwupload_mc_upload(struct ngl_node *node,
                            struct hwupload_config *config,
                            struct sxplayer_frame *frame);

int ngli_hwupload_mc_dr_init(struct ngl_node *node,
                             struct hwupload_config *config);

int ngli_hwupload_mc_dr_upload(struct ngl_node *node,
                               struct hwupload_config *config,
                               struct sxplayer_frame *frame);

#endif
