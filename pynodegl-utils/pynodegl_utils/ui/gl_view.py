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

from PyQt5 import QtCore, QtWidgets
from PyQt5.QtCore import Qt
from PyQt5.QtCore import QEvent

from seekbar import Seekbar

from pynodegl_utils import player
from pynodegl_utils import export


class _GLWidget(QtWidgets.QWidget):

    on_player_available = QtCore.pyqtSignal(name='onPlayerAvailable')

    def __init__(self, parent, config):
        super(_GLWidget, self).__init__(parent)

        self.setAttribute(Qt.WA_DontCreateNativeAncestors)
        self.setAttribute(Qt.WA_NativeWindow)
        self.setAttribute(Qt.WA_PaintOnScreen)
        self.setMinimumSize(640, 360)

        self._player = None
        self._last_frame_time = 0.0
        self._config = config

    def paintEngine(self):
        return None

    def resizeEvent(self, event):
        if not self._player:
            return

        size = event.size()
        width = int(size.width() * self.devicePixelRatioF())
        height = int(size.height() * self.devicePixelRatioF())
        self._player.resize(width, height)

        super(_GLWidget, self).resizeEvent(event)

    def event(self, event):
        if event.type() == QEvent.Paint:
            if not self._player:
                self._player = player.Player(
                    self.winId(),
                    self.width() * self.devicePixelRatioF(),
                    self.height() * self.devicePixelRatioF(),
                    self._config,
                )
                self._player.start()
                self._player.onFrame.connect(self._set_last_frame_time)
                self.onPlayerAvailable.emit()
            else:
                self._player.draw()
        elif event.type() == QEvent.Close:
            if self._player:
                self._player.stop()
                self._player.wait()
        return super(_GLWidget, self).event(event)

    @QtCore.pyqtSlot(int, float)
    def _set_last_frame_time(self, frame_index, frame_time):
        self._last_frame_time = frame_time

    def get_last_frame_time(self):
        return self._last_frame_time

    def get_player(self):
        return self._player


class GLView(QtWidgets.QWidget):

    def __init__(self, get_scene_func, config):
        super(GLView, self).__init__()

        self._get_scene_func = get_scene_func
        self._cfg = None

        self._seekbar = Seekbar(config)
        self._gl_widget = _GLWidget(self, config)
        self._gl_widget.onPlayerAvailable.connect(self._connect_seekbar)

        screenshot_btn = QtWidgets.QToolButton()
        screenshot_btn.setText(u'📷')

        toolbar = QtWidgets.QHBoxLayout()
        toolbar.addWidget(self._seekbar)
        toolbar.addWidget(screenshot_btn)

        self._gl_layout = QtWidgets.QVBoxLayout(self)
        self._gl_layout.addWidget(self._gl_widget, stretch=1)
        self._gl_layout.addLayout(toolbar)

        screenshot_btn.clicked.connect(self._screenshot)

    @QtCore.pyqtSlot()
    def _connect_seekbar(self):
        player = self._gl_widget.get_player()
        player.set_scene(self._cfg)

        player.onPlay.connect(self._seekbar.set_play_state)
        player.onPause.connect(self._seekbar.set_pause_state)
        player.onSceneMetadata.connect(self._seekbar.set_scene_metadata)
        player.onFrame.connect(self._seekbar.set_frame_time)

        self._seekbar.seek.connect(player.seek)
        self._seekbar.play.connect(player.play)
        self._seekbar.pause.connect(player.pause)
        self._seekbar.step.connect(player.step)
        self._seekbar.stop.connect(player.reset_scene)

    @QtCore.pyqtSlot()
    def _screenshot(self):
        filenames = QtWidgets.QFileDialog.getSaveFileName(self, 'Save screenshot file')
        if not filenames[0]:
            return
        exporter = export.Exporter(
            self._get_scene_func,
            filenames[0],
            self._gl_widget.width(),
            self._gl_widget.height(),
            ['-frames:v', '1'],
            self._gl_widget.get_last_frame_time()
        )
        exporter.start()
        exporter.wait()

    @QtCore.pyqtSlot(tuple)
    def set_aspect_ratio(self, ar):
        player = self._gl_widget.get_player()
        if not player:
            return
        player.set_aspect_ratio(ar)

    @QtCore.pyqtSlot(tuple)
    def set_frame_rate(self, fr):
        player = self._gl_widget.get_player()
        if not player:
            return
        player.set_framerate(fr)

    @QtCore.pyqtSlot(int)
    def set_samples(self, samples):
        player = self._gl_widget.get_player()
        if not player:
            return
        player.set_samples(samples)

    @QtCore.pyqtSlot(tuple)
    def set_clear_color(self, color):
        player = self._gl_widget.get_player()
        if not player:
            return
        player.set_clear_color(color)

    @QtCore.pyqtSlot(str)
    def set_backend(self, backend):
        player = self._gl_widget.get_player()
        if not player:
            return
        player.set_backend(backend)

    def enter(self):
        self._cfg = self._get_scene_func()
        if not self._cfg:
            return

        player = self._gl_widget.get_player()
        if not player:
            return
        player.set_scene(self._cfg)

        self._gl_widget.update()

    def leave(self):
        player = self._gl_widget.get_player()
        if not player:
            return
        player.pause()

    def closeEvent(self, close_event):
        self._gl_widget.close()
        self._seekbar.close()
        super(GLView, self).closeEvent(close_event)
