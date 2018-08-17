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
import subprocess

from PyQt5 import QtCore


class Hooks(QtCore.QObject):

    def __init__(self, get_scene_func, hooksdir):
        super(Hooks, self).__init__()
        self._hooksdir = hooksdir
        self._get_scene_func = get_scene_func

    def _get_hook(self, name):
        if not self._hooksdir:
            return None
        hook = op.join(self._hooksdir, 'hook.' + name)
        if not op.exists(hook):
            return
        return hook

    def _get_hook_output(self, name):
        hook = self._get_hook(name)
        if not hook:
            return None
        return subprocess.check_output([hook]).rstrip()

    def submit(self, module_name, scene_name):

        def filename_escape(filename):
            s = ''
            for c in filename:
                cval = ord(c)
                if cval >= ord('!') and cval <= '~' and cval != '%':
                    s += c
                else:
                    s += '%%%02x' % (cval & 0xff)
            return s

        def get_remotefile(filename, remotedir):
            statinfo = os.stat(filename)
            sha256 = hashlib.sha256()
            sha256.update(filename)
            sha256.update(str(statinfo.st_size))
            sha256.update(str(statinfo.st_mtime))
            digest = sha256.hexdigest()
            _, ext = op.splitext(filename)
            return op.join(remotedir, digest + ext)

        def uint_clear_color(vec4_color):
            uint_color = 0
            for i, comp in enumerate(vec4_color):
                comp_val = int(round(comp*0xff)) & 0xff
                uint_color |= comp_val << (24 - i*8)
            return uint_color

        try:
            # Bail out immediately if there is no script to run when a scene change
            # occurs
            hook_scene_change = self._get_hook('scene_change')
            if not hook_scene_change:
                return

            # The graphic backend can be different when using hooks: the scene might
            # be rendered on a remote device different from the one constructing
            # the scene graph
            backend = self._get_hook_output('get_gl_backend')
            system = self._get_hook_output('get_system')
            cfg = self._get_scene_func(backend=backend, system=system)
            if not cfg:
                return

            # The serialized scene is associated with a bunch of assets which we
            # need to sync. Similarly, the remote assets directory might be
            # different from the one in local, so we need to fix up the scene
            # appropriately.
            serialized_scene = cfg['scene']
            hook_sync = self._get_hook('sync')
            remotedir = self._get_hook_output('get_remote_dir')
            if hook_sync and remotedir:
                filelist = [m.filename for m in cfg['medias']] + cfg['files']
                for localfile in filelist:
                    remotefile = get_remotefile(localfile, remotedir)
                    serialized_scene = serialized_scene.replace(
                            filename_escape(localfile),
                            filename_escape(remotefile))
                    subprocess.check_call([hook_sync, localfile, remotefile])

            # The serialized scene is then stored in a file which is then
            # communicated with additional parameters to the user
            local_scene = '/tmp/ngl_scene.ngl'
            open(local_scene, 'w').write(serialized_scene)
            args = [hook_scene_change, local_scene,
                    'duration=%f' % cfg['duration'],
                    'framerate=%d/%d' % cfg['framerate'],
                    'aspect_ratio=%d/%d' % cfg['aspect_ratio'],
                    'clear_color=%08X' % uint_clear_color(cfg['clear_color']),
                    'samples=%d' % cfg['samples']]
            subprocess.check_call(args)

        except subprocess.CalledProcessError, e:
            QtWidgets.QMessageBox.critical(self, 'Hook error',
                                           'Error (%d) while executing %s' % (e.returncode, ' '.join(e.cmd)),
                                           QtWidgets.QMessageBox.Ok)
