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

import os
import os.path as op
import json
from PyQt5 import QtCore


class Config(QtCore.QObject):

    def _get_config_filepath(self):
        config_basedir = os.environ.get('XDG_DATA_HOME', op.expanduser('~/.local/share'))
        config_dir = op.join(config_basedir, 'node.gl')
        return op.join(config_dir, 'viewer.json')

    @QtCore.pyqtSlot()
    def _check_config(self):
        if not self._needs_saving:
            return

        config_filepath = self._get_config_filepath()
        config_dir = op.dirname(config_filepath)
        if not op.exists(config_dir):
            os.makedirs(config_dir)

        config_file = open(config_filepath, 'w')
        json.dump(self._cfg, config_file, indent=4)
        config_file.close()

        self._needs_saving = False

    def get(self, key):
        ret = self._cfg.get(key)
        return tuple(ret) if isinstance(ret, list) else ret

    def _set_cfg(self, key, value):
        self._cfg[key] = value
        self._needs_saving = True

    @QtCore.pyqtSlot(tuple)
    def set_aspect_ratio(self, ar):
        self._set_cfg('aspect_ratio', ar)

    @QtCore.pyqtSlot(tuple)
    def set_frame_rate(self, fr):
        self._set_cfg('framerate', fr)

    @QtCore.pyqtSlot(int)
    def _set_samples(self, samples):
        self._set_cfg('samples', samples)

    def geometry_changed(self, geometry):
        self._set_cfg('geometry', geometry)

    @QtCore.pyqtSlot(str, str)
    def scene_changed(self, module_name, scene_name):
        self._set_cfg('pkg', self._module_pkgname)
        self._set_cfg('module', module_name)
        self._set_cfg('scene',  scene_name)

    def __init__(self, cfg_defaults, module_pkgname):
        super(Config, self).__init__()

        self._cfg = cfg_defaults.copy()
        self._module_pkgname = module_pkgname
        self._needs_saving = False

        config_filepath = self._get_config_filepath()
        if op.exists(config_filepath):
            self._cfg = json.load(open(config_filepath, 'r'))
        else:
            self._needs_saving = True

        self._config_timer = QtCore.QTimer()
        self._config_timer.setInterval(1000) # every second
        self._config_timer.timeout.connect(self._check_config)
        self._config_timer.start()
