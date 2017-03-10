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

#
# User configuration
#
PREFIX     ?= /usr/local
SHARED     ?= no
DEBUG      ?= no
PKG_CONFIG ?= pkg-config
PYTHON     ?= python
TARGET_OS  ?= $(shell uname -s)


#
# Project configuration
#
PROJECT_NAME = nodegl
SPECS_FILE = nodes.specs
PROJECT_CFLAGS := $(CFLAGS) -Wall -O2 -Werror=missing-prototypes -std=c99
ifeq ($(DEBUG),yes)
	PROJECT_CFLAGS += -g
endif
PROJECT_LDLIBS := $(LDLIBS)


#
# Library configuration
#
LD_SYM_FILE   = $(LIB_BASENAME).symexport
LD_SYM_OPTION = --version-script
LD_SYM_DATA   = "{\n\tglobal: ngl_*;\n\tlocal: *;\n};\n"
DYLIBSUFFIX = so
ifeq ($(TARGET_OS),$(filter $(TARGET_OS),Darwin iPhone))
	DYLIBSUFFIX = dylib
	LD_SYM_OPTION = -exported_symbols_list
	LD_SYM_DATA   = "_ngl_*\n"
endif
ifeq ($(SHARED),yes)
	LIBSUFFIX = $(DYLIBSUFFIX)
else
	LIBSUFFIX = a
endif
LIB_BASENAME = lib$(PROJECT_NAME)
LIB_NAME     = $(LIB_BASENAME).$(LIBSUFFIX)
LIB_PCNAME   = $(LIB_BASENAME).pc

LIB_OBJS = api.o                    \
           bstr.o                   \
           dot.o                    \
           gl_utils.o               \
           glcontext.o              \
           log.o                    \
           math_utils.o             \
           node_animkeyframe.o      \
           node_attribute.o         \
           node_camera.o            \
           node_fps.o               \
           node_glblendstate.o      \
           node_glstate.o           \
           node_group.o             \
           node_identity.o          \
           node_media.o             \
           node_quad.o              \
           node_renderrange.o       \
           node_rotate.o            \
           node_rtt.o               \
           node_scale.o             \
           node_shader.o            \
           node_shape.o             \
           node_shapeprimitive.o    \
           node_texture.o           \
           node_texturedshape.o     \
           node_translate.o         \
           node_triangle.o          \
           node_uniform.o           \
           nodes.o                  \
           params.o                 \
           serialize.o              \
           transforms.o             \
           utils.o                  \

LIB_EXTRA_OBJS_Linux   = glcontext_x11.o
LIB_EXTRA_OBJS_Darwin  = glcontext_cgl.o
LIB_EXTRA_OBJS_Android = glcontext_egl.o
LIB_EXTRA_OBJS_iPhone  = glcontext_eagl.o

LIB_CFLAGS               = -fPIC -DTARGET_$(shell echo $(TARGET_OS) | tr a-z A-Z)
LIB_EXTRA_CFLAGS_Linux   = -DHAVE_PLATFORM_GLX
LIB_EXTRA_CFLAGS_Darwin  = -DHAVE_PLATFORM_CGL
LIB_EXTRA_CFLAGS_Android = -DHAVE_PLATFORM_EGL
LIB_EXTRA_CFLAGS_iPhone  = -DHAVE_PLATFORM_EAGL

LIB_LDLIBS               = -lm -lpthread
LIB_EXTRA_LDLIBS_Linux   =
LIB_EXTRA_LDLIBS_Darwin  = -framework OpenGL -framework CoreVideo -framework CoreFoundation
LIB_EXTRA_LDLIBS_Android =
LIB_EXTRA_LDLIBS_iPhone  = -framework OpenGLES -framework CoreMedia

LIB_PKG_CONFIG_LIBS               = "libsxplayer >= 8.0.0"
LIB_EXTRA_PKG_CONFIG_LIBS_Linux   = x11 gl
LIB_EXTRA_PKG_CONFIG_LIBS_Darwin  =
LIB_EXTRA_PKG_CONFIG_LIBS_Android = egl glesv2
LIB_EXTRA_PKG_CONFIG_LIBS_iPhone  =

LIB_OBJS   += $(LIB_EXTRA_OBJS_$(TARGET_OS))
LIB_CFLAGS += $(LIB_EXTRA_CFLAGS_$(TARGET_OS))
LIB_LDLIBS += $(LIB_EXTRA_LDLIBS_$(TARGET_OS))
LIB_CFLAGS += $(shell $(PKG_CONFIG) --cflags $(LIB_PKG_CONFIG_LIBS) $(LIB_EXTRA_PKG_CONFIG_LIBS_$(TARGET_OS)))
LIB_LDLIBS += $(shell $(PKG_CONFIG) --libs   $(LIB_PKG_CONFIG_LIBS) $(LIB_EXTRA_PKG_CONFIG_LIBS_$(TARGET_OS)))

LIB_DEPS = $(LIB_OBJS:.o=.d)


#
# Demo configuration
#
DEMO_OBJS = demo.o
DEMO_CFLAGS = $(shell $(PKG_CONFIG) --cflags glfw3) -I.
DEMO_LDLIBS = $(shell $(PKG_CONFIG) --libs   glfw3)
ifeq ($(SHARED),no)
DEMO_LDLIBS += $(LIB_LDLIBS)
endif


#
# gen_specs configuration
#
GENSPECS_OBJS = gen_specs.o $(LIB_OBJS)
GENSPECS_CFLAGS = $(LIB_CFLAGS)
GENSPECS_LDLIBS = $(LIB_LDLIBS)


#
# build rules
#
all: $(LIB_PCNAME) $(LIB_NAME)

$(LIB_NAME): CFLAGS  = $(PROJECT_CFLAGS) $(LIB_CFLAGS)
$(LIB_NAME): LDLIBS  = $(PROJECT_LDLIBS) $(LIB_LDLIBS)
$(LIB_NAME): LDFLAGS = -Wl,$(LD_SYM_OPTION),$(LD_SYM_FILE)
$(LIB_NAME): CPPFLAGS += -MMD -MP
$(LIB_NAME): $(LD_SYM_FILE) $(LIB_OBJS)
ifeq ($(SHARED),yes)
	$(CC) $(LDFLAGS) $(LIB_OBJS) -shared -o $@ $(LDLIBS)
else
	$(AR) rcs $@ $(LIB_OBJS)
endif

demo: CFLAGS = $(PROJECT_CFLAGS) $(DEMO_CFLAGS)
demo: LDLIBS = $(PROJECT_LDLIBS) $(DEMO_LDLIBS)
demo: $(DEMO_OBJS) $(LIB_NAME)

gen_specs: CFLAGS = $(PROJECT_CFLAGS) $(GENSPECS_CFLAGS)
gen_specs: LDLIBS = $(PROJECT_LDLIBS) $(GENSPECS_LDLIBS)
gen_specs: $(GENSPECS_OBJS)


#
# Misc rules
#
$(LD_SYM_FILE):
	$(shell printf $(LD_SYM_DATA) > $(LD_SYM_FILE))

$(LIB_PCNAME): $(LIB_PCNAME).tpl
ifeq ($(SHARED),yes)
	sed -e "s#PREFIX#$(PREFIX)#;s#DEP_LIBS##;s#DEP_PRIVATE_LIBS#$(LDLIBS)#" $^ > $@
else
	sed -e "s#PREFIX#$(PREFIX)#;s#DEP_LIBS#$(LDLIBS)#;s#DEP_PRIVATE_LIBS##" $^ > $@
endif

updatespecs: gen_specs
	./gen_specs > $(SPECS_FILE)


#
# Project rules
#
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
	$(RM) $(LIB_BASENAME).so $(LIB_BASENAME).dylib $(LIB_BASENAME).a
	$(RM) $(LIB_OBJS) $(LIB_DEPS)
	$(RM) $(DEMO_OBJS) demo
	$(RM) $(GENSPECS_OBJS) gen_specs
	$(RM) examples/*.pyc
	$(RM) $(LIB_PCNAME)
	$(RM) $(LD_SYM_FILE)

install: $(LIB_NAME) $(LIB_PCNAME)
	install -d $(DESTDIR)$(PREFIX)/lib
	install -d $(DESTDIR)$(PREFIX)/lib/pkgconfig
	install -d $(DESTDIR)$(PREFIX)/include
	install -d $(DESTDIR)$(PREFIX)/share
	install -d $(DESTDIR)$(PREFIX)/share/$(PROJECT_NAME)
	install -m 644 $(LIB_NAME) $(DESTDIR)$(PREFIX)/lib
	install -m 644 $(LIB_PCNAME) $(DESTDIR)$(PREFIX)/lib/pkgconfig
	install -m 644 $(PROJECT_NAME).h $(DESTDIR)$(PREFIX)/include/$(PROJECT_NAME).h
	install -m 644 $(SPECS_FILE) $(DESTDIR)$(PREFIX)/share/$(PROJECT_NAME)

uninstall:
	$(RM) $(DESTDIR)$(PREFIX)/lib/$(LIB_NAME)
	$(RM) $(DESTDIR)$(PREFIX)/include/$(PROJECT_NAME).h
	$(RM) -r $(DESTDIR)$(PREFIX)/share/$(PROJECT_NAME)

.PHONY: all updatespecs cleanpy clean install uninstall

-include $(LIB_DEPS)
