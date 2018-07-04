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
import subprocess
import sys
import os.path as op
from PyQt5 import QtCore, QtGui, QtWidgets

from pynodegl_utils.com import query_subproc
from pynodegl_utils.config import Config
from pynodegl_utils.misc import Media
from pynodegl_utils.scriptsmgr import ScriptsManager

from pynodegl_utils.ui.gl_view import GLView
from pynodegl_utils.ui.graph_view import GraphView
from pynodegl_utils.ui.export_view import ExportView
from pynodegl_utils.ui.serial_view import SerialView
from pynodegl_utils.ui.toolbar import Toolbar


class MainWindow(QtWidgets.QSplitter):

    error = QtCore.pyqtSignal(str)

    def __init__(self, module_pkgname, assets_dir, backend, hooksdir):
        super(MainWindow, self).__init__(QtCore.Qt.Horizontal)
        self._win_title_base = 'Node.gl viewer'
        self.setWindowTitle(self._win_title_base)

        self._module_pkgname = module_pkgname
        self._backend = backend
        self._scripts_mgr = ScriptsManager(module_pkgname)
        self._hooksdir = hooksdir

        medias = None
        if assets_dir:
            medias = []
            for f in sorted(os.listdir(assets_dir)):
                ext = f.rsplit('.', 1)[-1].lower()
                path = op.join(assets_dir, f)
                if op.isfile(path) and ext in ('mp4', 'mkv', 'avi', 'webm', 'mov'):
                    try:
                        media = Media(path)
                    except:
                        pass
                    else:
                        medias.append(media)

        self._medias = medias

        get_scene_func = self._get_scene

        self._config = Config(module_pkgname)

        # Apply previous geometry (position + dimensions)
        rect = self._config.get('geometry')
        if rect:
            geometry = QtCore.QRect(*rect)
            self.setGeometry(geometry)

        gl_view = GLView(get_scene_func, self._config)
        graph_view = GraphView(get_scene_func, self._config)
        export_view = ExportView(get_scene_func, self._config)
        serial_view = SerialView(get_scene_func)

        self._tabs = [
            ('Player view', gl_view),
            ('Graph view', graph_view),
            ('Export', export_view),
            ('Serialization', serial_view),
        ]
        self._last_tab_index = -1

        self._tab_widget = QtWidgets.QTabWidget()
        for tab_name, tab_view in self._tabs:
            self._tab_widget.addTab(tab_view, tab_name)
        self._tab_widget.currentChanged.connect(self._currentTabChanged)

        self._scene_toolbar = Toolbar(self._config)
        self._scene_toolbar.sceneChanged.connect(self._scene_changed)
        self._scene_toolbar.sceneChanged.connect(self._scene_changed_hook)
        self._scene_toolbar.sceneChanged.connect(self._config.scene_changed)
        self._scene_toolbar.aspectRatioChanged.connect(gl_view.set_aspect_ratio)
        self._scene_toolbar.aspectRatioChanged.connect(export_view.set_aspect_ratio)
        self._scene_toolbar.aspectRatioChanged.connect(self._config.set_aspect_ratio)
        self._scene_toolbar.samplesChanged.connect(gl_view.set_samples)
        self._scene_toolbar.samplesChanged.connect(self._config.set_samples)
        self._scene_toolbar.frameRateChanged.connect(gl_view.set_frame_rate)
        self._scene_toolbar.frameRateChanged.connect(graph_view.set_frame_rate)
        self._scene_toolbar.frameRateChanged.connect(export_view.set_frame_rate)
        self._scene_toolbar.frameRateChanged.connect(self._config.set_frame_rate)
        self._scene_toolbar.logLevelChanged.connect(self._config.set_log_level)
        self._scene_toolbar.clearColorChanged.connect(gl_view.set_clear_color)
        self._scene_toolbar.clearColorChanged.connect(self._config.set_clear_color)
        self._scene_toolbar.hudChanged.connect(self._config.set_hud)

        self._errbuf = QtWidgets.QPlainTextEdit()
        self._errbuf.setFont(QtGui.QFontDatabase.systemFont(QtGui.QFontDatabase.FixedFont))
        self._errbuf.setReadOnly(True)
        self._errbuf.hide()

        tabs_and_errbuf = QtWidgets.QVBoxLayout()
        tabs_and_errbuf.addWidget(self._tab_widget)
        tabs_and_errbuf.addWidget(self._errbuf)
        tabs_and_errbuf_widget = QtWidgets.QWidget()
        tabs_and_errbuf_widget.setLayout(tabs_and_errbuf)

        self.addWidget(self._scene_toolbar)
        self.addWidget(tabs_and_errbuf_widget)
        self.setStretchFactor(1, 1)

        self._scene_toolbar.reload_btn.clicked.connect(self._scripts_mgr.reload)  # TODO: drop
        self._scripts_mgr.error.connect(self._all_scripts_err)
        self._scripts_mgr.scriptsChanged.connect(self._scene_toolbar.on_scripts_changed)
        self._scripts_mgr.start()

        self.error.connect(self._scene_err)

        # Load the previous scene if the current and previously loaded
        # module packages match
        prev_pkgname = self._config.get('pkg')
        prev_module = self._config.get('module')
        prev_scene = self._config.get('scene')
        if prev_pkgname == module_pkgname:
            self._scene_toolbar.load_scene_from_name(prev_module, prev_scene)

    @QtCore.pyqtSlot(str)
    def _scene_err(self, err_str):
        if err_str:
            self._errbuf.setPlainText(err_str)
            self._errbuf.show()
            sys.stderr.write(err_str)
        else:
            self._errbuf.hide()

    @QtCore.pyqtSlot(str)
    def _all_scripts_err(self, err_str):
        self._scene_toolbar.clear_scripts()
        self._scene_err(err_str)

    def _get_scene(self, **cfg_overrides):
        cfg = self._scene_toolbar.get_cfg()
        if cfg['scene'] is None:
            return None
        cfg['backend'] = self._backend
        cfg['pkg'] = self._module_pkgname
        cfg['medias'] = self._medias
        cfg.update(cfg_overrides)

        self._scripts_mgr.pause()
        ret = query_subproc(query='scene', **cfg)
        if 'error' in ret:
            self._scripts_mgr.resume()
            self.error.emit(ret['error'])
            return None

        self.error.emit(None)
        self._scripts_mgr.set_filelist(ret['filelist'])
        self._scripts_mgr.resume()
        self._scene_toolbar.set_cfg(ret)

        return ret

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

    @QtCore.pyqtSlot(str, str)
    def _scene_changed(self, module_name, scene_name):
        self.setWindowTitle('%s - %s.%s' % (self._win_title_base, module_name, scene_name))
        self._currentTabChanged(self._tab_widget.currentIndex())

    @QtCore.pyqtSlot(str, str)
    def _scene_changed_hook(self, module_name, scene_name):

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
            cfg = self._get_scene(backend=backend, system=system)
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

    def _emit_geometry(self):
        geometry = (self.x(), self.y(), self.width(), self.height())
        self._config.geometry_changed(geometry)

    @QtCore.pyqtSlot(QtGui.QResizeEvent)
    def resizeEvent(self, resize_event):
        super(MainWindow, self).resizeEvent(resize_event)
        self._emit_geometry()

    @QtCore.pyqtSlot(QtGui.QMoveEvent)
    def moveEvent(self, move_event):
        super(MainWindow, self).moveEvent(move_event)
        self._emit_geometry()

    @QtCore.pyqtSlot(QtGui.QCloseEvent)
    def closeEvent(self, close_event):
        for name, widget in self._tabs:
            widget.close()
        super(MainWindow, self).closeEvent(close_event)

    @QtCore.pyqtSlot(int)
    def _currentTabChanged(self, index):
        next_view = self._tabs[index][1]
        prev_view = self._tabs[self._last_tab_index][1]
        if index != self._last_tab_index and hasattr(prev_view, 'leave'):
            prev_view.leave()
        if hasattr(next_view, 'enter'):
            next_view.enter()
        self._last_tab_index = index
