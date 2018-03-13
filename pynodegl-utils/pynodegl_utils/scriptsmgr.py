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

from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler

from PyQt5 import QtCore

from com import query_subproc


class ScriptsManager(QtCore.QObject):

    scripts_changed = QtCore.pyqtSignal(list, name='scriptsChanged')
    error = QtCore.pyqtSignal(str)

    def __init__(self, module_pkgname):
        super(ScriptsManager, self).__init__()
        self._module_pkgname = module_pkgname
        self._dirs_to_watch = set()
        self._files_to_watch = set()

        self._event_handler = FileSystemEventHandler()
        self._event_handler.on_any_event = self._on_any_event
        self._observer = Observer()
        self._observer.start()

    def start(self):
        self.reload()

    @QtCore.pyqtSlot()
    def reload(self):
        odict = query_subproc(query='list', pkg=self._module_pkgname)
        if 'error' in odict:
            self.error.emit(odict['error'])
            return
        self.set_filelist(odict['filelist'])

        scripts = odict['scenes']
        self.scripts_changed.emit(scripts)

    def _on_any_event(self, event):
        if event.src_path in self._files_to_watch:
            print('Change in %s detected, reload scene' % event.src_path)
            self.reload()

    @QtCore.pyqtSlot(list)
    def set_filelist(self, filelist):
        self._files_to_watch = set(filelist)
        self._dirs_to_watch = set(op.dirname(f) for f in self._files_to_watch)

        self._observer.unschedule_all()
        for path in self._dirs_to_watch:
            self._observer.schedule(self._event_handler, path)
