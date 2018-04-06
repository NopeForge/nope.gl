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

import subprocess
from PyQt5 import QtCore, QtGui, QtWidgets, QtSvg

from seekbar import Seekbar

import pynodegl as ngl


class _SVGGraphView(QtWidgets.QGraphicsView):

    def wheelEvent(self, event):
        y_degrees = event.angleDelta().y() / 8.0
        self._zoom_level += y_degrees / 15.0  # in the most common mouse step unit
        m = QtGui.QTransform()
        factor = 1.25 ** self._zoom_level
        m.scale(factor, factor)
        self.setTransform(m)

    def __init__(self):
        super(_SVGGraphView, self).__init__()
        self.setDragMode(QtWidgets.QGraphicsView.ScrollHandDrag)
        self._zoom_level = 0


class GraphView(QtWidgets.QWidget):

    @QtCore.pyqtSlot()
    def _save_to_file(self):
        filenames = QtWidgets.QFileDialog.getSaveFileName(self, 'Select export file')
        if not filenames[0]:
            return
        rect_size = self._scene.sceneRect().size()
        img_size = QtCore.QSize(int(rect_size.width()), int(rect_size.height()))
        img = QtGui.QImage(img_size, QtGui.QImage.Format_ARGB32_Premultiplied)
        painter = QtGui.QPainter(img)
        self._scene.render(painter)
        img.save(filenames[0])
        painter.end()

    def enter(self):
        cfg_overrides = {}
        if not self._seek_chkbox.isChecked():
            cfg_overrides['fmt'] = 'dot'

        cfg = self._get_scene_func(**cfg_overrides)
        if not cfg:
            return

        self._seekbar.set_duration(cfg['duration'])

        if self._seek_chkbox.isChecked():
            self._glctx.makeCurrent(self._surface)
            self._viewer.set_scene_from_string(cfg['scene'])
            self._glctx.doneCurrent()
            self._seekbar.refresh()
        else:
            dot_scene = cfg['scene']
            self._update_graph(dot_scene)

    def leave(self):
        self._seekbar.pause()

    def _disable_timed_scene(self):
        if not self._viewer:
            return
        self._glctx.makeCurrent(self._surface)
        self._fbo.release()
        del self._viewer
        self._glctx.doneCurrent()
        del self._surface
        self._fbo = self._viewer = self._surface = None

    def _enable_timed_scene(self):

        # GL context
        glctx = QtGui.QOpenGLContext()
        assert glctx.create() is True
        assert glctx.isValid() is True

        # Offscreen Surface
        surface = QtGui.QOffscreenSurface()
        surface.create()
        assert surface.isValid() is True

        glctx.makeCurrent(surface)

        # Framebuffer
        fbo_format = QtGui.QOpenGLFramebufferObjectFormat()
        fbo_format.setAttachment(QtGui.QOpenGLFramebufferObject.CombinedDepthStencil)
        fbo = QtGui.QOpenGLFramebufferObject(16, 16, fbo_format)
        assert fbo.isValid() is True
        fbo.bind()

        # node.gl context
        ngl_viewer = ngl.Viewer()
        ngl_viewer.configure(ngl.GLPLATFORM_AUTO, ngl.GLAPI_AUTO)

        glctx.doneCurrent()

        self._viewer = ngl_viewer
        self._glctx = glctx
        self._surface = surface
        self._fbo = fbo

    def _update_graph(self, dot_scene):
        basename = '/tmp/ngl_scene.'
        dotfile = basename + 'dot'
        svgfile = basename + 'svg'
        open(dotfile, 'w').write(dot_scene)
        try:
            subprocess.call(['dot', '-Tsvg', dotfile, '-o' + svgfile])
        except OSError, e:
            QtWidgets.QMessageBox.critical(self, 'Graphviz error',
                                           'Error while executing dot (Graphviz): %s' % e.strerror,
                                           QtWidgets.QMessageBox.Ok)
            return

        item = QtSvg.QGraphicsSvgItem(svgfile)
        self._scene.clear()
        self._scene.addItem(item)
        self._scene.setSceneRect(item.boundingRect())

    @QtCore.pyqtSlot(tuple)
    def set_frame_rate(self, fr):
        self._seekbar.set_framerate(fr)

    @QtCore.pyqtSlot(int)
    def set_samples(self, samples):
        self._samples = samples

    @QtCore.pyqtSlot(int)
    def _seek_check_changed(self, state):
        if state:
            self._seekbar.setEnabled(True)
            self._enable_timed_scene()
        else:
            self._seekbar.pause()
            self._seekbar.setEnabled(False)
            self._disable_timed_scene()
        self.enter()

    def _time_changed(self, time):
        if not self._viewer:
            return
        self._glctx.makeCurrent(self._surface)
        dot_scene = self._viewer.dot(time)
        self._glctx.doneCurrent()
        if dot_scene:
            self._update_graph(dot_scene)

    def __init__(self, get_scene_func, config):
        super(GraphView, self).__init__()

        self._get_scene_func = get_scene_func
        self._samples = config.get('samples')
        self._viewer = None

        self._save_btn = QtWidgets.QPushButton("Save image")

        self._scene = QtWidgets.QGraphicsScene()
        self._view = _SVGGraphView()
        self._view.setScene(self._scene)

        self._seek_chkbox = QtWidgets.QCheckBox('Show graph at a given time')
        self._seekbar = Seekbar(config.get('framerate'), stop_button=False)
        self._seekbar.setEnabled(False)

        hbox = QtWidgets.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget(self._save_btn)

        graph_layout = QtWidgets.QVBoxLayout(self)
        graph_layout.addWidget(self._seek_chkbox)
        graph_layout.addWidget(self._seekbar)
        graph_layout.addWidget(self._view)
        graph_layout.addLayout(hbox)

        self._save_btn.clicked.connect(self._save_to_file)
        self._seek_chkbox.stateChanged.connect(self._seek_check_changed)
        self._seekbar.timeChanged.connect(self._time_changed)
