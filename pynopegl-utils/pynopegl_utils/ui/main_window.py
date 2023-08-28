#
# Copyright 2018-2022 GoPro Inc.
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
from typing import Optional

from pynopegl_utils.com import query_scene
from pynopegl_utils.config import Config
from pynopegl_utils.hooks import HooksCaller, HooksController
from pynopegl_utils.misc import SceneCfg, SceneInfo
from pynopegl_utils.scriptsmgr import ScriptsManager
from pynopegl_utils.ui.export_view import ExportView
from pynopegl_utils.ui.graph_view import GraphView
from pynopegl_utils.ui.hooks_view import HooksView
from pynopegl_utils.ui.medias_view import MediasView
from pynopegl_utils.ui.serial_view import SerialView
from pynopegl_utils.ui.toolbar import Toolbar
from PySide6 import QtCore, QtGui, QtWidgets


class MainWindow(QtWidgets.QSplitter):
    sceneLoaded = QtCore.Signal(SceneInfo)
    error = QtCore.Signal(str)

    def __init__(self, module_pkgname, hooks_scripts):
        super().__init__(QtCore.Qt.Horizontal)
        self._win_title_base = "Nope.gl controller"
        self.setWindowTitle(self._win_title_base)

        get_scene_func = self._get_scene

        self._module_pkgname = module_pkgname
        self._scripts_mgr = ScriptsManager(module_pkgname)
        self._hooks_caller = HooksCaller(hooks_scripts)
        self._hooks_ctl = HooksController(get_scene_func, self._hooks_caller)

        self._config = Config(module_pkgname)

        # Apply previous geometry (position + dimensions)
        rect = self._config.get("geometry")
        if rect:
            geometry = QtCore.QRect(*rect)
            self.setGeometry(geometry)

        graph_view = GraphView(get_scene_func, self._config)
        export_view = ExportView(get_scene_func, self._config)
        hooks_view = HooksView(self._hooks_ctl, self._config)
        self._medias_view = MediasView(self._config)
        serial_view = SerialView(get_scene_func)

        self._tabs = [
            ("Hooks", hooks_view),
            ("Graph view", graph_view),
            ("Export", export_view),
            ("Medias", self._medias_view),
            ("Serialization", serial_view),
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
        self._scene_toolbar.aspectRatioChanged.connect(self._config.set_aspect_ratio)
        self._scene_toolbar.samplesChanged.connect(self._config.set_samples)
        self._scene_toolbar.frameRateChanged.connect(self._config.set_frame_rate)
        self._scene_toolbar.logLevelChanged.connect(self._config.set_log_level)
        self._scene_toolbar.clearColorChanged.connect(self._config.set_clear_color)
        self._scene_toolbar.backendChanged.connect(self._config.set_backend)

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
        self.sceneLoaded.connect(self._scene_loaded)

        # Load the previous scene if the current and previously loaded
        # module packages match
        prev_pkgname = self._config.get("pkg")
        prev_module = self._config.get("module")
        prev_scene = self._config.get("scene")
        if prev_pkgname == module_pkgname:
            self._scene_toolbar.load_scene_from_name(prev_module, prev_scene)

    @QtCore.Slot(str)
    def _scene_err(self, err_str):
        if err_str:
            self._errbuf.setPlainText(err_str)
            self._errbuf.show()
            sys.stderr.write(err_str)
        else:
            self._errbuf.hide()

    @QtCore.Slot(str)
    def _all_scripts_err(self, err_str):
        self._scene_toolbar.clear_scripts()
        self._scene_err(err_str)

    def _get_scene(self, backend=None, system=None) -> Optional[SceneInfo]:
        cfg = self._scene_toolbar.get_cfg()
        scene_func, extra_args = self._scene_toolbar.get_scene_info()
        if scene_func is None:
            return None
        medias = self._medias_view.get_medias()
        if medias:
            # Replace the default medias with user medias only if the user has
            # specified some
            cfg.medias = medias

        # Config overrides
        if backend is not None:
            cfg.backend = backend
        if system is not None:
            cfg.system = system

        self._scripts_mgr.inc_query_count()
        self._scripts_mgr.pause()
        query_info = query_scene(self._module_pkgname, scene_func, cfg, extra_args)
        self._scripts_mgr.update_filelist(query_info.filelist)
        self._scripts_mgr.update_modulelist(query_info.modulelist)
        self._scripts_mgr.resume()
        self._scripts_mgr.dec_query_count()

        if query_info.error is not None:
            self.error.emit(query_info.error)
            return None

        scene = query_info.ret
        self.error.emit(None)
        self.sceneLoaded.emit(scene)

        return scene

    @QtCore.Slot(str, str)
    def _scene_changed(self, module_name, scene_name):
        self.setWindowTitle(f"{self._win_title_base} - {module_name}.{scene_name}")
        self._currentTabChanged(self._tab_widget.currentIndex())

    @QtCore.Slot(str, str)
    def _scene_changed_hook(self, module_name, scene_name):
        self._hooks_ctl.process(module_name, scene_name)

    @QtCore.Slot(dict)
    def _scene_loaded(self, scene_info: SceneInfo):
        self._scene_toolbar.set_scene_info(scene_info)

    def _emit_geometry(self):
        geometry = (self.x(), self.y(), self.width(), self.height())
        self._config.geometry_changed(geometry)

    @QtCore.Slot(QtGui.QResizeEvent)
    def resizeEvent(self, resize_event):
        super().resizeEvent(resize_event)
        self._emit_geometry()

    @QtCore.Slot(QtGui.QMoveEvent)
    def moveEvent(self, move_event):
        super().moveEvent(move_event)
        self._emit_geometry()

    @QtCore.Slot(QtGui.QCloseEvent)
    def closeEvent(self, close_event):
        self._hooks_ctl.stop_threads()
        for name, widget in self._tabs:
            widget.close()
        super().closeEvent(close_event)

    @QtCore.Slot(int)
    def _currentTabChanged(self, index):
        next_view = self._tabs[index][1]
        prev_view = self._tabs[self._last_tab_index][1]
        if index != self._last_tab_index and hasattr(prev_view, "leave"):
            prev_view.leave()
        if hasattr(next_view, "enter"):
            next_view.enter()
        self._last_tab_index = index
