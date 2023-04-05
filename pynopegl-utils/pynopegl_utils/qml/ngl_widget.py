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

from PySide6 import QtCore
from PySide6.QtGui import QColor, QOpenGLContext, QOpenGLFunctions
from PySide6.QtOpenGL import QOpenGLFramebufferObject, QOpenGLFramebufferObjectFormat
from PySide6.QtQml import QmlElement
from PySide6.QtQuick import QQuickFramebufferObject

import pynopegl as ngl

QML_IMPORT_NAME = "QMLNopeGL"
QML_IMPORT_MAJOR_VERSION = 1
QML_IMPORT_MINOR_VERSION = 0


@QmlElement
class NopeGLWidget(QQuickFramebufferObject):
    livectls_changed = QtCore.Signal(object)

    _NODE_TYPES_MODEL_MAP = dict(
        UniformColor="color",
        UniformBool="bool",
        UniformVec2="vec2",
        UniformVec3="vec3",
        UniformVec4="vec4",
        UniformMat4="mat4",
        Text="text",
    )

    def __init__(self, parent=None):
        super().__init__(parent)
        self._scene = None
        self.setMirrorVertically(True)

        # Fields used for sync with the renderer
        self.t = 0.0
        self.request_stop = False
        self.request_scene = None
        self.livectls_changes = {}

    def createRenderer(self):
        self._renderer = _NopeGLRenderer(self.window())

        livectls = ngl.get_livectls(self._scene)
        if livectls:
            model_data = []
            for label, ctl in livectls.items():
                data = dict(
                    type=self._NODE_TYPES_MODEL_MAP.get(ctl["node_type"], "range"),
                    label=label,
                    val=ctl["val"],
                    node=ctl["node"],
                )
                if data["type"] not in ("bool", "text"):
                    data["min"] = ctl["min"]
                    data["max"] = ctl["max"]
                if data["type"] == "color":
                    data["val"] = QColor.fromRgbF(*ctl["val"])
                model_data.append(data)
            self.livectls_changes = {}
            self.livectls_changed.emit(model_data)

        self.request_scene = self._scene

        return self._renderer

    def set_scene(self, scene):
        self._scene = scene

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
                backend=ngl.BACKEND_OPENGL,
                width=self._fbo.width(),
                height=self._fbo.height(),
                backend_config=config_gl,
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
        self._context.resize(w, h, (0, 0, w, h))

        for live_id, ctl in self._livectls_changes.items():
            node = ctl["node"]
            type_ = ctl["type"]
            value = ctl["val"]
            if type_ in ("range", "bool"):
                node.set_value(value)
            elif type_ == "color":
                node.set_value(*QColor.getRgbF(value)[:3])
            elif type_ == "text":
                node.set_text(value)
            elif type_.startswith("vec") or type_.startswith("mat"):
                node.set_value(*value)
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
