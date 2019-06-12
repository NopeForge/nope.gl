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

        # From the Python 2 documentation (as of 2019/06/12): "When the name
        # variable is of the form package.module, normally, the top-level
        # package (the name up till the first dot) is returned, not the module
        # named by name.  However, when a non-empty fromlist argument is given,
        # the module named by name is returned."
        #
        # Source: https://docs.python.org/2/library/functions.html#__import__
        #
        # This means that at this point, `ret` *may* be the parent module:
        # Doing "import foo.bar.baz", you get ret=<module 'foo'>. In this
        # situation, we can only look for our child module in sys.modules.
        #
        # But beware, we must look into sys.modules ONLY in certain situations:
        # Indeed, if foo.bar.baz is imported, sometimes we only get "baz" in
        # the name instead. This will cause a wrong lookup into sys.modules
        # ("baz" instead of "foo.bar.baz") and may break havok.
        #
        # One example of such a scenario is with the relative import of
        # "exceptions" in the attr modules. The hook only sees
        # name="exceptions" (with ret=<module 'attr.exceptions'>), and looking
        # up "exceptions" in sys.modules will return the built-in module
        # instead of attr.exceptions.
        #
        # To identify if `ret` is the top-level package module or the child,
        # one may innocently look for a '.' in the name. Indeed, the
        # documentation mentions "the form package.module", but this is
        # followed by the word "normally". This means that in practice, it's
        # not always the case. One example is with "from .foo.bar import baz".
        # In this situation, we get name="foo.bar", but since this is a
        # relative import, "foo.bar" will not exist in sys.modules. Instead, we
        # decide to rely on the `fromlist` variable (see the last sentence of
        # the documentation quote).

        mods = [ret]
        if not fromlist:
            mods.append(sys.modules[name])

        for mod in mods:
           if self._mod_is_blacklisted(mod):
               continue
           path = mod.__file__
           if path.endswith('.pyc'):
               path = path[:-1]
           self.filelist.update([op.realpath(path)])
        return ret

    def _open_hook(self, name, mode='r', buffering=-1):
        ret = self._builtin_open(name, mode, buffering)
        self.filelist.update([op.realpath(name)])
        return ret

    def start_hooking(self):
        __builtin__.__import__ = self._import_hook
        __builtin__.open = self._open_hook

    def end_hooking(self):
        __builtin__.__import__ = self._builtin_import
        __builtin__.open = self._builtin_open
