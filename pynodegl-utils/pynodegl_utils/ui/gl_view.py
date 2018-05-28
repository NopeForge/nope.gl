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

from fractions import Fraction
from PyQt5 import QtCore, QtGui, QtWidgets

from seekbar import Seekbar

import pynodegl as ngl


class _GLWidget(QtWidgets.QOpenGLWidget):

    def __init__(self, parent, aspect_ratio, samples, clear_color):
        super(_GLWidget, self).__init__(parent)

        self.setMinimumSize(640, 360)
        self._viewer = None
        self._scene = None
        self._time = 0
        self._aspect_ratio = aspect_ratio
        self._samples = samples
        self._clear_color = clear_color
        gl_format = QtGui.QSurfaceFormat.defaultFormat()
        gl_format.setSamples(samples)
        self.setFormat(gl_format)
        self.resizeGL(self.width(), self.height())

    def get_time(self):
        return self._time

    def set_time(self, t):
        self._time = t
        self.update()

    def reset_viewer(self):
        self.makeCurrent()
        del self._viewer
        self._viewer = None
        del self._scene
        self._scene = None
        self.initializeGL()
        self.doneCurrent()
        self.update()

    def set_scene(self, scene):
        self._scene = scene
        if not self._viewer:
            return
        self.makeCurrent()
        self._viewer.set_scene_from_string(scene)
        self.doneCurrent()
        self.update()

    def paintGL(self):
        self._viewer.set_viewport(self.view_x, self.view_y, self.view_width, self.view_height)
        self._viewer.draw(self._time)

    def resizeGL(self, screen_width, screen_height):
        screen_width = int(screen_width * self.devicePixelRatioF())
        screen_height = int(screen_height * self.devicePixelRatioF())
        aspect = self._aspect_ratio
        self.view_width = screen_width
        self.view_height = screen_width * aspect[1] / aspect[0]

        if self.view_height > screen_height:
            self.view_height = screen_height
            self.view_width = screen_height * aspect[0] / aspect[1]

        self.view_x = (screen_width - self.view_width) // 2
        self.view_y = (screen_height - self.view_height) // 2

    def initializeGL(self):
        api = ngl.GLAPI_OPENGL
        if self.context().isOpenGLES():
            api = ngl.GLAPI_OPENGLES
        self._viewer = ngl.Viewer()
        self._viewer.configure(platform=ngl.GLPLATFORM_AUTO, api=api)
        self._viewer.set_clearcolor(*self._clear_color)
        if self._scene:
            self._viewer.set_scene_from_string(self._scene)


class GLView(QtWidgets.QWidget):

    def __init__(self, get_scene_func, config):
        super(GLView, self).__init__()

        self._ar = config.get('aspect_ratio')
        self._samples = config.get('samples')
        self._clear_color = config.get('clear_color')

        self._get_scene_func = get_scene_func

        self._gl_widget = _GLWidget(self, self._ar, self._samples, self._clear_color)
        self._seekbar = Seekbar(config.get('framerate'))

        screenshot_btn = QtWidgets.QToolButton()
        screenshot_btn.setText(u'📷')

        toolbar = QtWidgets.QHBoxLayout()
        toolbar.addWidget(self._seekbar)
        toolbar.addWidget(screenshot_btn)

        self._gl_layout = QtWidgets.QVBoxLayout(self)
        self._gl_layout.addWidget(self._gl_widget, stretch=1)
        self._gl_layout.addLayout(toolbar)

        screenshot_btn.clicked.connect(self._screenshot)

        self._seekbar.timeChanged.connect(self._time_changed)
        self._seekbar.stopped.connect(self._stopped)

    @QtCore.pyqtSlot()
    def _stopped(self):
        self._gl_widget.reset_viewer()

    @QtCore.pyqtSlot()
    def _screenshot(self):
        filenames = QtWidgets.QFileDialog.getSaveFileName(self, 'Save screenshot file')
        if not filenames[0]:
            return
        self._gl_widget.grabFramebuffer().save(filenames[0])

    def _recreate_gl_widget(self):
        gl_widget = _GLWidget(self, self._ar, self._samples, self._clear_color)
        gl_widget.set_time(self._gl_widget.get_time())
        self._gl_layout.replaceWidget(self._gl_widget, gl_widget)
        self._gl_widget.setParent(None)
        self._gl_widget = gl_widget

    @QtCore.pyqtSlot(tuple)
    def set_aspect_ratio(self, ar):
        self._ar = ar
        self._recreate_gl_widget()

    @QtCore.pyqtSlot(tuple)
    def set_frame_rate(self, fr):
        self._seekbar.set_framerate(fr)

    @QtCore.pyqtSlot(int)
    def set_samples(self, samples):
        self._samples = samples
        self._recreate_gl_widget()

    @QtCore.pyqtSlot(tuple)
    def set_clear_color(self, color):
        self._clear_color = color
        self._recreate_gl_widget()

    def enter(self):
        cfg = self._get_scene_func()
        if not cfg:
            return
        if Fraction(*cfg['aspect_ratio']) != Fraction(*self._ar):
            self.set_aspect_ratio(cfg['aspect_ratio'])
        self._gl_widget.set_scene(cfg['scene'])
        self._seekbar.set_duration(cfg['duration'])

    def leave(self):
        self._seekbar.pause()

    @QtCore.pyqtSlot(float)
    def _time_changed(self, t):
        self._gl_widget.set_time(t)
