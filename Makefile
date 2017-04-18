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
else
ifeq ($(TARGET_OS),MinGW-w64)
	DYLIBSUFFIX = dll
	EXESUF = .exe
endif # MinGW
endif # Darwin/iPhone

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
           deserialize.o            \
           dot.o                    \
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
LIB_EXTRA_OBJS_MinGW-w64 = glcontext_wgl.o

LIB_CFLAGS               = -fPIC -DTARGET_$(shell echo $(TARGET_OS) | tr a-z- A-Z_)
LIB_EXTRA_CFLAGS_Linux   = -DHAVE_PLATFORM_GLX
LIB_EXTRA_CFLAGS_Darwin  = -DHAVE_PLATFORM_CGL
LIB_EXTRA_CFLAGS_Android = -DHAVE_PLATFORM_EGL
LIB_EXTRA_CFLAGS_iPhone  = -DHAVE_PLATFORM_EAGL
LIB_EXTRA_CFLAGS_MinGW-w64 = -DHAVE_PLATFORM_WGL

LIB_LDLIBS               = -lm -lpthread
LIB_EXTRA_LDLIBS_Linux   =
LIB_EXTRA_LDLIBS_Darwin  = -framework OpenGL -framework CoreVideo -framework CoreFoundation
LIB_EXTRA_LDLIBS_Android = -legl
LIB_EXTRA_LDLIBS_iPhone  = -framework CoreMedia
LIB_EXTRA_LDLIBS_MinGW-w64 = -lopengl32

LIB_PKG_CONFIG_LIBS               = "libsxplayer >= 8.1.1"
LIB_EXTRA_PKG_CONFIG_LIBS_Linux   = x11 gl
LIB_EXTRA_PKG_CONFIG_LIBS_Darwin  =
LIB_EXTRA_PKG_CONFIG_LIBS_Android =
LIB_EXTRA_PKG_CONFIG_LIBS_iPhone  =

LIB_OBJS   += $(LIB_EXTRA_OBJS_$(TARGET_OS))
LIB_CFLAGS += $(LIB_EXTRA_CFLAGS_$(TARGET_OS))
LIB_LDLIBS += $(LIB_EXTRA_LDLIBS_$(TARGET_OS))
LIB_CFLAGS += $(shell $(PKG_CONFIG) --cflags $(LIB_PKG_CONFIG_LIBS) $(LIB_EXTRA_PKG_CONFIG_LIBS_$(TARGET_OS)))
LIB_LDLIBS += $(shell $(PKG_CONFIG) --libs   $(LIB_PKG_CONFIG_LIBS) $(LIB_EXTRA_PKG_CONFIG_LIBS_$(TARGET_OS)))

LIB_DEPS = $(LIB_OBJS:.o=.d)


#
# Player configuration
#
PLAYER_OBJS = ngl-player.o
PLAYER_CFLAGS = $(shell $(PKG_CONFIG) --cflags libsxplayer) -I.
PLAYER_LDLIBS = $(shell $(PKG_CONFIG) --libs   libsxplayer)
ifeq ($(TARGET_OS),MinGW-w64)
PLAYER_LDLIBS += -lopengl32 -lglfw3
else
PLAYER_CFLAGS += $(shell $(PKG_CONFIG) --cflags glfw3)
PLAYER_LDLIBS += $(shell $(PKG_CONFIG) --libs   glfw3)
endif
ifeq ($(SHARED),no)
PLAYER_LDLIBS += $(LIB_LDLIBS)
endif


#
# Render tool configuration
#
RENDER_OBJS = ngl-render.o
ifeq ($(TARGET_OS),MinGW-w64)
RENDER_CFLAGS = -I.
RENDER_LDLIBS = -lopengl32 -lglfw3
else
RENDER_CFLAGS = $(shell $(PKG_CONFIG) --cflags glfw3) -I.
RENDER_LDLIBS = $(shell $(PKG_CONFIG) --libs   glfw3)
endif
ifeq ($(SHARED),no)
RENDER_LDLIBS += $(LIB_LDLIBS)
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


ngl-player$(EXESUF): CFLAGS = $(PROJECT_CFLAGS) $(PLAYER_CFLAGS)
ngl-player$(EXESUF): LDLIBS = $(PROJECT_LDLIBS) $(PLAYER_LDLIBS)
ngl-player$(EXESUF): $(PLAYER_OBJS) $(LIB_NAME)
TOOLS += ngl-player$(EXESUF)

ngl-render$(EXESUF): CFLAGS = $(PROJECT_CFLAGS) $(RENDER_CFLAGS)
ngl-render$(EXESUF): LDLIBS = $(PROJECT_LDLIBS) $(RENDER_LDLIBS)
ngl-render$(EXESUF): $(RENDER_OBJS) $(LIB_NAME)
TOOLS += ngl-render$(EXESUF)

gen_specs$(EXESUF): CFLAGS = $(PROJECT_CFLAGS) $(GENSPECS_CFLAGS)
gen_specs$(EXESUF): LDLIBS = $(PROJECT_LDLIBS) $(GENSPECS_LDLIBS)
gen_specs$(EXESUF): $(GENSPECS_OBJS)
TOOLS += gen_specs$(EXESUF)

$(TOOLS):
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

tools: $(TOOLS)

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

updatespecs: gen_specs$(EXESUF)
	./gen_specs$(EXESUF) > $(SPECS_FILE)


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
	$(RM) $(LIB_BASENAME).so $(LIB_BASENAME).dylib $(LIB_BASENAME).a $(LIB_BASENAME).dll
	$(RM) $(LIB_OBJS) $(LIB_DEPS)
	$(RM) $(PLAYER_OBJS) ngl-player$(EXESUF)
	$(RM) $(RENDER_OBJS) ngl-render$(EXESUF)
	$(RM) $(GENSPECS_OBJS) gen_specs$(EXESUF)
	$(RM) examples/*.pyc
	$(RM) $(LIB_PCNAME)
	$(RM) $(LD_SYM_FILE)

install: $(LIB_NAME) $(LIB_PCNAME)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -d $(DESTDIR)$(PREFIX)/lib
	install -d $(DESTDIR)$(PREFIX)/lib/pkgconfig
	install -d $(DESTDIR)$(PREFIX)/include
	install -d $(DESTDIR)$(PREFIX)/share
	install -d $(DESTDIR)$(PREFIX)/share/$(PROJECT_NAME)
ifeq ($(TARGET_OS),MinGW-w64)
	install -m 644 $(LIB_NAME) $(DESTDIR)$(PREFIX)/bin
endif
	install -m 644 $(LIB_NAME) $(DESTDIR)$(PREFIX)/lib
	install -m 644 $(LIB_PCNAME) $(DESTDIR)$(PREFIX)/lib/pkgconfig
	install -m 644 $(PROJECT_NAME).h $(DESTDIR)$(PREFIX)/include/$(PROJECT_NAME).h
	install -m 644 $(SPECS_FILE) $(DESTDIR)$(PREFIX)/share/$(PROJECT_NAME)

uninstall:
	$(RM) $(DESTDIR)$(PREFIX)/lib/$(LIB_NAME)
	$(RM) $(DESTDIR)$(PREFIX)/include/$(PROJECT_NAME).h
	$(RM) -r $(DESTDIR)$(PREFIX)/share/$(PROJECT_NAME)

tests_serial:
	$(PYTHON) ./tests/serialize.py data

tests: tests_serial ngl-render
	@for f in tests/data/*.ngl; do \
		./ngl-render $$f -t 3:2:5 -t 0:1:60 -t 7:3:15; \
	done

.PHONY: all updatespecs cleanpy clean install uninstall tests tests_serial

-include $(LIB_DEPS)
