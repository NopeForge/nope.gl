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

#ifndef NODES_REGISTER_H
#define NODES_REGISTER_H

#define NODE_MAP_TYPE2CLASS(action)                                             \
    action(NGL_NODE_ANIMATIONSCALAR,        ngli_animationscalar_class)         \
    action(NGL_NODE_ANIMATIONVEC2,          ngli_animationvec2_class)           \
    action(NGL_NODE_ANIMATIONVEC3,          ngli_animationvec3_class)           \
    action(NGL_NODE_ANIMATIONVEC4,          ngli_animationvec4_class)           \
    action(NGL_NODE_ANIMKEYFRAMESCALAR,     ngli_animkeyframescalar_class)      \
    action(NGL_NODE_ANIMKEYFRAMEVEC2,       ngli_animkeyframevec2_class)        \
    action(NGL_NODE_ANIMKEYFRAMEVEC3,       ngli_animkeyframevec3_class)        \
    action(NGL_NODE_ANIMKEYFRAMEVEC4,       ngli_animkeyframevec4_class)        \
    action(NGL_NODE_BUFFERSCALAR,           ngli_bufferscalar_class)            \
    action(NGL_NODE_BUFFERUINT,             ngli_bufferuint_class)              \
    action(NGL_NODE_BUFFERVEC2,             ngli_buffervec2_class)              \
    action(NGL_NODE_BUFFERVEC3,             ngli_buffervec3_class)              \
    action(NGL_NODE_BUFFERVEC4,             ngli_buffervec4_class)              \
    action(NGL_NODE_CAMERA,                 ngli_camera_class)                  \
    action(NGL_NODE_FPS,                    ngli_fps_class)                     \
    action(NGL_NODE_GEOMETRY,               ngli_geometry_class)                \
    action(NGL_NODE_GLBLENDSTATE,           ngli_glblendstate_class)            \
    action(NGL_NODE_GLCOLORSTATE,           ngli_glcolorstate_class)            \
    action(NGL_NODE_GLSTATE,                ngli_glstate_class)                 \
    action(NGL_NODE_GLSTENCILSTATE,         ngli_glstencilstate_class)          \
    action(NGL_NODE_GROUP,                  ngli_group_class)                   \
    action(NGL_NODE_IDENTITY,               ngli_identity_class)                \
    action(NGL_NODE_MEDIA,                  ngli_media_class)                   \
    action(NGL_NODE_PROGRAM,                ngli_program_class)                 \
    action(NGL_NODE_QUAD,                   ngli_quad_class)                    \
    action(NGL_NODE_RENDER,                 ngli_render_class)                  \
    action(NGL_NODE_RENDERRANGECONTINUOUS,  ngli_renderrangecontinuous_class)   \
    action(NGL_NODE_RENDERRANGENORENDER,    ngli_renderrangenorender_class)     \
    action(NGL_NODE_RENDERRANGEONCE,        ngli_renderrangeonce_class)         \
    action(NGL_NODE_RENDERTOTEXTURE,        ngli_rtt_class)                     \
    action(NGL_NODE_ROTATE,                 ngli_rotate_class)                  \
    action(NGL_NODE_SCALE,                  ngli_scale_class)                   \
    action(NGL_NODE_TEXTURE,                ngli_texture_class)                 \
    action(NGL_NODE_TRANSLATE,              ngli_translate_class)               \
    action(NGL_NODE_TRIANGLE,               ngli_triangle_class)                \
    action(NGL_NODE_UNIFORMINT,             ngli_uniformint_class)              \
    action(NGL_NODE_UNIFORMMAT4,            ngli_uniformmat4_class)             \
    action(NGL_NODE_UNIFORMSCALAR,          ngli_uniformscalar_class)           \
    action(NGL_NODE_UNIFORMVEC2,            ngli_uniformvec2_class)             \
    action(NGL_NODE_UNIFORMVEC3,            ngli_uniformvec3_class)             \
    action(NGL_NODE_UNIFORMVEC4,            ngli_uniformvec4_class)             \

#endif
