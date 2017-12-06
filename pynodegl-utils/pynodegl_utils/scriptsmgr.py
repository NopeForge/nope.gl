#!/usr/bin/env python
# -*- coding: utf-8 -*-
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

import os.path as op
import imp
import importlib
import pkgutil
import traceback
import distutils.sysconfig
import __builtin__

from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler

from PyQt5 import QtCore

class ScriptsManager(QtCore.QObject):

    MODULES_BLACKLIST = [
        'numpy',
        'threading',
        'watchdog',
    ]

    scripts_changed = QtCore.pyqtSignal(list, name='scriptsChanged')
    error = QtCore.pyqtSignal(str)

    def __init__(self, module_pkgname):
        super(ScriptsManager, self).__init__()
        self._module_is_script = module_pkgname.endswith('.py')
        self._module_pkgname = module_pkgname
        if self._module_is_script:
            self._module_pkgname = op.realpath(self._module_pkgname)
        self._builtin_import = __builtin__.__import__
        self._builtin_open = __builtin__.open
        self._dirs_to_watch = set()
        self._files_to_watch = set()
        self._modules_to_reload = set()
        self._pysysdir = op.realpath(distutils.sysconfig.get_python_lib(standard_lib=True))

        self._event_handler = FileSystemEventHandler()
        self._event_handler.on_any_event = self._on_any_event
        self._observer = Observer()
        self._observer.start()

    def _mod_is_blacklisted(self, mod):
        if not hasattr(mod, '__file__'):
            return True

        modpath = op.realpath(op.dirname(mod.__file__))
        if modpath.startswith(self._pysysdir):
            return True

        modname = mod.__name__
        for bl_mod in self.MODULES_BLACKLIST:
            if modname.startswith(bl_mod):
                return True

        return False

    def start(self):
        self._reload_scripts(initial_import=True)

    @QtCore.pyqtSlot()
    def reload(self):
        self._reload_scripts()

    def _on_any_event(self, event):
        if event.src_path in self._files_to_watch:
            print('Change in %s detected, reload scene' % event.src_path)
            self.reload()

    def start_hooking(self):
        __builtin__.__import__ = self._import_hook
        __builtin__.open = self._open_hook

    def end_hooking(self):
        __builtin__.__import__ = self._builtin_import
        __builtin__.open = self._builtin_open

        self._observer.unschedule_all()
        for path in self._dirs_to_watch:
            self._observer.schedule(self._event_handler, path)

    def _queue_watch_path(self, path):
        self._dirs_to_watch.update([op.dirname(path)])
        if path.endswith('.pyc'):
            path = path[:-1]
        self._files_to_watch.update([path])

    def _load_script(self, path):
        dname = op.dirname(path)
        fname = op.basename(path)
        name = fname[:-3]
        fp, pathname, description = imp.find_module(name, [dname])
        try:
            return imp.load_module(name, fp, pathname, description)
        finally:
            if fp:
                fp.close()

    def _reload_unsafe(self, initial_import):

        modules_to_reload = self._modules_to_reload.copy()
        for i, module in enumerate(modules_to_reload):
            reload(module)

        if initial_import:
            if self._module_is_script:
                self._module = self._load_script(self._module_pkgname)
            else:
                self._module = importlib.import_module(self._module_pkgname)
            self._queue_watch_path(self._module.__file__)

        scripts = []
        if self._module_is_script:
            if not initial_import:
                self._module = self._load_script(self._module_pkgname) # reload
            self._queue_watch_path(self._module_pkgname)
            scripts.append((self._module.__name__, self._module))
        else:
            for module in pkgutil.iter_modules(self._module.__path__):
                module_finder, module_name, ispkg = module
                script = importlib.import_module('.' + module_name, self._module_pkgname)
                if not initial_import:
                    reload(script)
                self._queue_watch_path(script.__file__)
                scripts.append((module_name, script))

        self.scripts_changed.emit(scripts)

    def _import_hook(self, name, globals={}, locals={}, fromlist=[], level=-1):
        ret = self._builtin_import(name, globals, locals, fromlist, level)
        if not self._mod_is_blacklisted(ret):
            self._queue_watch_path(ret.__file__)
            self._modules_to_reload.update([ret])
        return ret

    def _open_hook(self, name, mode="r", buffering=-1):
        ret = self._builtin_open(name, mode, buffering)
        self._queue_watch_path(name)
        return ret

    def _reload_scripts(self, initial_import=False):
        self.start_hooking()
        try:
            self._reload_unsafe(initial_import)
        except:
            self.error.emit(traceback.format_exc())
        self.end_hooking()
