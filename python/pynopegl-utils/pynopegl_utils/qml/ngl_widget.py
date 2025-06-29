#
# Copyright 2022 GoPro Inc.
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

import pynopegl as ngl
from pynopegl_utils.qml import livectls
from PySide6 import QtCore
from PySide6.QtGui import QOpenGLContext, QOpenGLFunctions
from PySide6.QtOpenGL import QOpenGLFramebufferObject, QOpenGLFramebufferObjectFormat
from PySide6.QtQml import QmlElement
from PySide6.QtQuick import QQuickFramebufferObject

QML_IMPORT_NAME = "QMLNopeGL"
QML_IMPORT_MAJOR_VERSION = 1
QML_IMPORT_MINOR_VERSION = 0


@QmlElement
class NopeGLWidget(QQuickFramebufferObject):
    livectls_changed = QtCore.Signal(object)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMirrorVertically(True)

        # Fields used for sync with the renderer
        self.t = 0.0
        self.request_stop = False
        self.request_scene = None
        self.livectls_changes = {}

    def createRenderer(self):
        self._renderer = _NopeGLRenderer(self.window())
        return self._renderer

    def set_scene(self, scene):
        model_data = livectls.get_model_data(scene)

        self.livectls_changes = {}
        self.livectls_changed.emit(model_data)

        self.request_scene = scene
        self.update()

    @QtCore.Slot(float)
    def set_time(self, t):
        if self.t == t:
            return
        self.t = t
        self.update()

    @QtCore.Slot()
    def stop(self):
        self.request_stop = True
        self.update()


class _NopeGLRenderer(QQuickFramebufferObject.Renderer):
    def __init__(self, window):
        super().__init__()
        self._t = 0.0
        self._context = None
        self._window = window
        self._fbo = None
        self._ar = (1, 1)

        self._request_stop = False
        self._request_scene = None
        self._livectls_changes = {}

    def createFramebufferObject(self, size):
        fmt = QOpenGLFramebufferObjectFormat()
        fmt.setAttachment(QOpenGLFramebufferObject.CombinedDepthStencil)
        self._fbo = QOpenGLFramebufferObject(size, fmt)

        # Clear any previous OpenGL error. This workarounds an issue on macOS
        # where the current context contains errors (due to Qt OpenGL
        # implementation). These errors need to be cleared before calling the
        # nope.gl API otherwise nope.gl will in turn error out.
        gl_funcs = QOpenGLFunctions(QOpenGLContext.currentContext())
        gl_funcs.glGetError()

        if not self._context:
            self._context = ngl.Context()
            config_gl = ngl.ConfigGL(external=True, external_framebuffer=self._fbo.handle())
            self._context.configure(
                ngl.Config(
                    backend=ngl.Backend.OPENGL,
                    width=self._fbo.width(),
                    height=self._fbo.height(),
                    backend_config=config_gl,
                )
            )
        else:
            self._context.gl_wrap_framebuffer(self._fbo.handle())

        return self._fbo

    def render(self):
        if not self._context:
            return

        self._window.beginExternalCommands()

        if self._request_stop:
            self._request_scene = None
            assert self._context.set_scene(None) == 0
            self._t = 0.0

        if self._request_scene:
            assert self._context.set_scene(self._request_scene) == 0
            self._request_scene = None

        w, h = self._fbo.width(), self._fbo.height()
        self._context.gl_wrap_framebuffer(self._fbo.handle())
        self._context.resize(w, h)

        livectls.apply_changes(list(self._livectls_changes.values()))
        self._livectls_changes = {}

        self._context.draw(self._t)

        if self._request_stop:
            del self._context
            self._context = None
            self._request_stop = False

        self._window.endExternalCommands()

    def synchronize(self, ngl_widget):
        self._t = ngl_widget.t

        self._request_stop = ngl_widget.request_stop
        self._request_scene = ngl_widget.request_scene
        self._livectls_changes = ngl_widget.livectls_changes

        ngl_widget.request_stop = False
        ngl_widget.request_scene = None
        ngl_widget.livectls_changes = {}
