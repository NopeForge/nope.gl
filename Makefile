#
# Copyright 2016 GoPro Inc.
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

NAME = nodegl

PROJECT_OBJS = api.o                    \
               bstr.o                   \
               dot.o                    \
               glcontext.o              \
               gl_utils.o               \
               log.o                    \
               math_utils.o             \
               nodes.o                  \
               node_attribute.o         \
               node_animkeyframe.o      \
               node_camera.o            \
               node_fps.o               \
               node_glstate.o           \
               node_glblendstate.o      \
               node_group.o             \
               node_media.o             \
               node_quad.o              \
               node_triangle.o          \
               node_shapeprimitive.o    \
               node_shape.o             \
               node_renderrange.o       \
               node_rotate.o            \
               node_rtt.o               \
               node_scale.o             \
               node_shader.o            \
               node_texture.o           \
               node_texturedshape.o     \
               node_translate.o         \
               node_uniform.o           \
               params.o                 \
               utils.o                  \

LINUX_OBJS   = glcontext_x11.o
DARWIN_OBJS  =
ANDROID_OBJS = glcontext_egl.o

PROJECT_PKG_CONFIG_LIBS = libsxplayer
LINUX_PKG_CONFIG_LIBS   = x11 gl
DARWIN_PKG_CONFIG_LIBS  =
ANDROID_PKG_CONFIG_LIBS = egl glesv2

PROJECT_LIBS = -lm -lpthread
DARWIN_LIBS  = -framework OpenGL -framework CoreVideo
ANDROID_LIBS =

PREFIX ?= /usr/local
PKG_CONFIG ?= pkg-config

SHARED ?= no
DEBUG  ?= no

PYTHON ?= python

TARGET_OS ?= $(shell uname -s)

PROJECT_LDFLAGS = -Wl,--version-script,lib$(NAME).ver

DYLIBSUFFIX = so
ifeq ($(TARGET_OS),Darwin)
	DYLIBSUFFIX = dylib
	PROJECT_LIBS            += $(DARWIN_LIBS)
	PROJECT_PKG_CONFIG_LIBS += $(DARWIN_PKG_CONFIG_LIBS)
	PROJECT_OBJS            += $(DARWIN_OBJS)
	PROJECT_LDFLAGS =
else
ifeq ($(TARGET_OS),Android)
	PROJECT_LIBS            += $(ANDROID_LIBS)
	PROJECT_PKG_CONFIG_LIBS += $(ANDROID_PKG_CONFIG_LIBS)
	PROJECT_OBJS            += $(ANDROID_OBJS)
	CFLAGS                  += -DHAVE_PLATFORM_EGL
else
ifeq ($(TARGET_OS),Linux)
	PROJECT_LIBS            += $(LINUX_LIBS)
	PROJECT_PKG_CONFIG_LIBS += $(LINUX_PKG_CONFIG_LIBS)
	PROJECT_OBJS            += $(LINUX_OBJS)
	CFLAGS                  += -DHAVE_PLATFORM_GLX
endif # linux
endif # android
endif # darwin

ifeq ($(SHARED),yes)
	LIBSUFFIX = $(DYLIBSUFFIX)
else
	LIBSUFFIX = a
endif

LIBNAME = lib$(NAME).$(LIBSUFFIX)
PCNAME  = lib$(NAME).pc
PYNAME  = py$(NAME)

OBJS += $(PROJECT_OBJS)

CPPFLAGS += -MMD -MP

CFLAGS += -Wall -O2 -Werror=missing-prototypes -fPIC -std=c11
ifeq ($(DEBUG),yes)
	CFLAGS += -g
endif
CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PROJECT_PKG_CONFIG_LIBS)) $(CFLAGS)
LDLIBS := $(shell $(PKG_CONFIG) --libs   $(PROJECT_PKG_CONFIG_LIBS)) $(LDLIBS) $(PROJECT_LIBS)
LDFLAGS := $(PROJECT_LDFLAGS)

ALLDEPS = $(OBJS:.o=.d)

all: $(PCNAME) $(LIBNAME)

demo: CFLAGS += -lnodegl -I. $(shell $(PKG_CONFIG) --cflags glfw3)
demo: LDLIBS += -L. $(shell $(PKG_CONFIG) --libs glfw3)
demo: demo.o $(LIBNAME)

SPECS_FILE = nodes.specs
gen_specs: gen_specs.o $(OBJS)
updatespecs: gen_specs
	./gen_specs > $(SPECS_FILE)

$(LIBNAME): $(OBJS)
ifeq ($(SHARED),yes)
	$(CC) $(LDFLAGS) $^ -shared -o $@ $(LDLIBS)
else
	$(AR) rcs $@ $^
endif

cleanpy:
	$(RM) pynodegl/nodes_def.pyx
	$(RM) pynodegl/pynodegl.c
	$(RM) pynodegl/pynodegl.so
	$(RM) -r pynodegl/build
	$(RM) -r pynodegl/pynodegl.egg-info
	$(RM) -r pynodegl/.eggs
	$(RM) -r pynodegl-utils/pynodegl_utils.egg-info
	$(RM) -r pynodegl-utils/.eggs

clean:
	$(RM) lib$(NAME).so lib$(NAME).dylib lib$(NAME).a
	$(RM) $(PYNAME).so $(PYNAME).c
	$(RM) $(OBJS) $(ALLDEPS)
	$(RM) examples/*.pyc
	$(RM) $(PCNAME)
	$(RM) demo.o demo
	$(RM) gen_specs gen_specs.o

$(PCNAME): $(PCNAME).tpl
ifeq ($(SHARED),yes)
	sed -e "s#PREFIX#$(PREFIX)#;s#DEP_LIBS##;s#DEP_PRIVATE_LIBS#$(LDLIBS)#" $^ > $@
else
	sed -e "s#PREFIX#$(PREFIX)#;s#DEP_LIBS#$(LDLIBS)#;s#DEP_PRIVATE_LIBS##" $^ > $@
endif

install: $(LIBNAME) $(PCNAME)
	install -d $(DESTDIR)$(PREFIX)/lib
	install -d $(DESTDIR)$(PREFIX)/lib/pkgconfig
	install -d $(DESTDIR)$(PREFIX)/include
	install -d $(DESTDIR)$(PREFIX)/share
	install -d $(DESTDIR)$(PREFIX)/share/$(NAME)
	install -m 644 $(LIBNAME) $(DESTDIR)$(PREFIX)/lib
	install -m 644 $(PCNAME) $(DESTDIR)$(PREFIX)/lib/pkgconfig
	install -m 644 $(NAME).h $(DESTDIR)$(PREFIX)/include/$(NAME).h
	install -m 644 $(SPECS_FILE) $(DESTDIR)$(PREFIX)/share/$(NAME)

uninstall:
	$(RM) $(DESTDIR)$(PREFIX)/lib/$(LIBNAME)
	$(RM) $(DESTDIR)$(PREFIX)/include/$(NAME).h
	$(RM) -r $(DESTDIR)$(PREFIX)/share/$(NAME)

.PHONY: all updatespecs cleanpy clean install uninstall

-include $(ALLDEPS)
