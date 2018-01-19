#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2018 GoPro Inc.
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

import sys
import __builtin__
import os.path as op
import distutils.sysconfig

class FileTracker:

    def __init__(self):
        self.filelist = set()

        self._builtin_import = __builtin__.__import__
        self._builtin_open = __builtin__.open
        self._pysysdir = op.realpath(distutils.sysconfig.get_python_lib(standard_lib=True))

    def _mod_is_blacklisted(self, mod):
        if not hasattr(mod, '__file__'):
            return True

        modpath = op.realpath(op.dirname(mod.__file__))
        if modpath.startswith(self._pysysdir):
            return True

        return False

    def _import_hook(self, name, globals={}, locals={}, fromlist=[], level=-1):
        ret = self._builtin_import(name, globals, locals, fromlist, level)
        if not self._mod_is_blacklisted(ret):
            mods = [ret]
            real_mod = sys.modules.get(name)
            if real_mod:
                mods.append(real_mod)
            for mod in mods:
                path = mod.__file__
                if path.endswith('.pyc'):
                    path = path[:-1]
                self.filelist.update([path])
        return ret

    def _open_hook(self, name, mode="r", buffering=-1):
        ret = self._builtin_open(name, mode, buffering)
        self.filelist.update([name])
        return ret

    def start_hooking(self):
        __builtin__.__import__ = self._import_hook
        __builtin__.open = self._open_hook

    def end_hooking(self):
        __builtin__.__import__ = self._builtin_import
        __builtin__.open = self._builtin_open
