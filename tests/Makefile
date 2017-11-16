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

VISUAL ?= no

all: tests

tests_serial:
	$(PYTHON) serialize.py data

ifeq ($(VISUAL),yes)
RENDER_FLAGS = -w -z 1
endif
tests: tests_serial
	@for f in data/*.ngl; do \
		ngl-render $$f -t 3:2:5 -t 0:1:60 -t 7:3:15 $(RENDER_FLAGS); \
	done

clean:
	$(RM) -r data

.PHONY: clean tests tests_serial all
