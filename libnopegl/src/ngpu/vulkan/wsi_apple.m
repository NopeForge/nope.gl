/*
 * Copyright 2025 Matthieu Bouron <matthieu.bouron@gmail.com>
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

#include "config.h"

#if defined(TARGET_DARWIN)
#import <Cocoa/Cocoa.h>
#elif defined(TARGET_IPHONE)
#import <UIKit/UIKit.h>
#endif
#import <QuartzCore/CAMetalLayer.h>

#import "log.h"
#import "nopegl.h"
#import "wsi_apple.h"

#if defined(TARGET_DARWIN)
static CAMetalLayer *get_metal_layer(NSObject *object)
{
    CALayer *layer = nil;
    if ([object isKindOfClass:[NSView class]]) {
        NSView *view = (NSView *)object;
        layer = view.layer;
    } else if ([object isKindOfClass:[NSWindow class]]) {
        NSWindow *window = (NSWindow *)object;
        NSView *view = [window contentView];
        layer = view != nil ? view.layer : nil;
    } else if ([object isKindOfClass:[CALayer class]]) {
        layer = (CALayer *)object;
    }
    if (layer == nil) {
        LOG(ERROR, "window must be either a NSView, a NSWindow or a CAMetalLayer");
        return nil;
    }

    if (![layer isKindOfClass:[CAMetalLayer class]]) {
        LOG(ERROR, "layer must be a CAMetalLayer");
        return NULL;
    }

    return (CAMetalLayer *)layer;
}
#endif

#if defined(TARGET_IPHONE)
static CAMetalLayer *get_metal_layer(NSObject *object)
{
    CALayer *layer = nil;
    if ([object isKindOfClass:[UIView class]]) {
        UIView *view = (UIView *)object;
        layer = view.layer;
    } else if ([object isKindOfClass:[CAMetalLayer class]]) {
        layer = (CALayer *)layer;
    }
    if (layer == nil) {
        LOG(ERROR, "window must be either a UIView or a CAMetalLayer");
        return nil;
    }

    if (![layer isKindOfClass:[CAMetalLayer class]]) {
        LOG(ERROR, "layer must be a CAMetalLayer");
        return NULL;
    }

    return (CAMetalLayer *)layer;
}
#endif

CAMetalLayer *ngpu_window_get_metal_layer(const void *handle)
{
    return get_metal_layer((NSObject *)handle);
}

