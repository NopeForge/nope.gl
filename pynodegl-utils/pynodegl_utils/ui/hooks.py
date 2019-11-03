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

import hashlib
import os
import os.path as op
import tempfile
import subprocess

from PyQt5 import QtCore


class HooksCaller:

    def __init__(self, hooksdir):
        self._hooksdir = hooksdir
        self.hooks_available = self._get_hook('scene_change') is not None

    @property
    def sync_available(self):
        return self._get_hook('sync') and self._get_hook_output('get_remote_dir')

    def _get_hook(self, name):
        if not self._hooksdir:
            return None
        hook = op.join(self._hooksdir, 'hook.' + name)
        if not op.exists(hook):
            return
        return hook

    def _get_hook_output(self, name, *args):
        hook = self._get_hook(name)
        if not hook:
            return None
        cmd = [hook] + list(args)
        return subprocess.check_output(cmd).rstrip()

    def get_gl_backend(self):
        return self._get_hook_output('get_gl_backend')

    def get_system(self):
        return self._get_hook_output('get_system')

    @staticmethod
    def _uint_clear_color(vec4_color):
        uint_color = 0
        for i, comp in enumerate(vec4_color):
            comp_val = int(round(comp*0xff)) & 0xff
            uint_color |= comp_val << (24 - i*8)
        return uint_color

    def scene_change(self, local_scene, cfg):
        self._get_hook_output(
            'scene_change',
            local_scene,
            'duration=%f' % cfg['duration'],
            'framerate=%d/%d' % cfg['framerate'],
            'aspect_ratio=%d/%d' % cfg['aspect_ratio'],
            'clear_color=%08X' % self._uint_clear_color(cfg['clear_color']),
            'samples=%d' % cfg['samples'],
        )

    @staticmethod
    def _get_remotefile(filename, remotedir):
        statinfo = os.stat(filename)
        sha256 = hashlib.sha256()
        sha256.update(filename)
        sha256.update(str(statinfo.st_size))
        sha256.update(str(statinfo.st_mtime))
        digest = sha256.hexdigest()
        _, ext = op.splitext(filename)
        return op.join(remotedir, digest + ext)

    def sync_file(self, localfile):
        remotedir = self._get_hook_output('get_remote_dir')
        remotefile = self._get_remotefile(localfile, remotedir)
        self._get_hook_output('sync', localfile, remotefile)
        return remotefile


class Hooks(QtCore.QThread):

    uploading_file_notif = QtCore.pyqtSignal(int, int, str, name='uploadingFileNotif')
    building_scene_notif = QtCore.pyqtSignal(str, str, name='buildingSceneNotif')
    sending_scene_notif = QtCore.pyqtSignal(name='sendingSceneNotif')
    error = QtCore.pyqtSignal(str)

    def __init__(self, get_scene_func, hooksdir):
        super(Hooks, self).__init__()
        self._hooks_caller = HooksCaller(hooksdir)
        self._get_scene_func = get_scene_func

    def __del__(self):
        self.wait()

    @staticmethod
    def _filename_escape(filename):
        s = ''
        for c in filename:
            cval = ord(c)
            if cval >= ord('!') and cval <= '~' and cval != '%':
                s += c
            else:
                s += '%%%02x' % (cval & 0xff)
        return s

    def run(self):

        if not self._hooks_caller.hooks_available:
            return

        try:
            # The graphic backend can be different when using hooks: the scene might
            # be rendered on a remote device different from the one constructing
            # the scene graph
            backend = self._hooks_caller.get_gl_backend()
            system = self._hooks_caller.get_system()
            self.buildingSceneNotif.emit(backend, system)
            cfg = self._get_scene_func(backend=backend, system=system)
            if not cfg:
                return

            # The serialized scene is associated with a bunch of assets which we
            # need to sync. Similarly, the remote assets directory might be
            # different from the one in local, so we need to fix up the scene
            # appropriately.
            serialized_scene = cfg['scene']
            if self._hooks_caller.sync_available:
                filelist = [m.filename for m in cfg['medias']] + cfg['files']
                for i, localfile in enumerate(filelist, 1):
                    self.uploadingFileNotif.emit(i, len(filelist), localfile)
                    remotefile = self._hooks_caller.sync_file(localfile)
                    serialized_scene = serialized_scene.replace(
                            self._filename_escape(localfile),
                            self._filename_escape(remotefile))

            # The serialized scene is then stored in a file which is then
            # communicated with additional parameters to the user
            local_scene = op.join(tempfile.gettempdir(), 'ngl_scene.ngl')
            open(local_scene, 'w').write(serialized_scene)
            self.sendingSceneNotif.emit()
            self._hooks_caller.scene_change(local_scene, cfg)

        except subprocess.CalledProcessError, e:
            self.error.emit('Error (%d) while executing %s' % (e.returncode, ' '.join(e.cmd)))
        except Exception, e:
            self.error.emit('Error executing hooks: %s' % str(e))
