#
# Copyright 2017 GoPro Inc.
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

include ../common.mak

TOOLS_CFLAGS = $(shell $(PKG_CONFIG) --cflags libnodegl libsxplayer)
TOOLS_LDLIBS = $(shell $(PKG_CONFIG) --libs   libnodegl libsxplayer)

# GLFW3
ifeq ($(TARGET_OS),MinGW-w64)
TOOLS_LDLIBS += -lglfw3
else
TOOLS_CFLAGS += $(shell $(PKG_CONFIG) --cflags glfw3)
TOOLS_LDLIBS += $(shell $(PKG_CONFIG) --libs   glfw3)
endif

# OpenGL
ifeq ($(TARGET_OS),MinGW-w64)
TOOLS_LDLIBS += -lopengl32
else
ifeq ($(TARGET_OS),Darwin)
TOOLS_LDLIBS += -framework OpenGL
else
TOOLS_CFLAGS += $(shell $(PKG_CONFIG) --cflags gl)
TOOLS_LDLIBS += $(shell $(PKG_CONFIG) --libs   gl)
endif
endif

HAS_PYTHON := $(if $(shell pkg-config --exists python2 && echo 1),yes,no)

TOOLS = ngl-player$(EXESUF) ngl-render$(EXESUF)
ifeq ($(HAS_PYTHON),yes)
TOOLS += ngl-python$(EXESUF)
endif

all: $(TOOLS)

ngl-player$(EXESUF): CFLAGS = $(PROJECT_CFLAGS) $(TOOLS_CFLAGS)
ngl-player$(EXESUF): LDLIBS = $(PROJECT_LDLIBS) $(TOOLS_LDLIBS)
ngl-player$(EXESUF): ngl-player.o

ngl-render$(EXESUF): CFLAGS = $(PROJECT_CFLAGS) $(TOOLS_CFLAGS)
ngl-render$(EXESUF): LDLIBS = $(PROJECT_LDLIBS) $(TOOLS_LDLIBS)
ngl-render$(EXESUF): ngl-render.o

ngl-python$(EXESUF): CFLAGS = $(PROJECT_CFLAGS) $(TOOLS_CFLAGS) $(shell python2-config --cflags)
ngl-python$(EXESUF): LDLIBS = $(PROJECT_LDLIBS) $(TOOLS_LDLIBS) $(shell python2-config --libs)
ngl-python$(EXESUF): ngl-python.o

$(TOOLS):
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	$(RM) ngl-player.o ngl-player$(EXESUF)
	$(RM) ngl-render.o ngl-render$(EXESUF)
	$(RM) ngl-python.o ngl-python$(EXESUF)

install: $(TOOLS)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TOOLS) $(DESTDIR)$(PREFIX)/bin

uninstall:
	$(RM) $(addprefix $(DESTDIR)$(PREFIX)/bin/, $(TOOLS))

.PHONY: all clean install uninstall
