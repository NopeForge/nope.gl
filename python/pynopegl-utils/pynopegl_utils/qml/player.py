#
# Copyright 2024 Clément Bœsch <u pkh.me>
# Copyright 2024 Nope Forge
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

import os.path as op

import pynopegl as ngl
from pynopegl_utils.qml import ngl_widget  # noqa: register NopeGLWidget as a QML element
from PySide6.QtCore import QObject, QUrl
from PySide6.QtQml import QQmlComponent


class NGLPlayer:
    def __init__(self, app_window, qml_engine):
        # Dynamically create an instance of the rendering widget
        widget_url = QUrl.fromLocalFile(op.join(op.dirname(__file__), "NopeGLWidget.qml"))
        component = QQmlComponent(qml_engine)
        component.loadUrl(widget_url)
        widget = component.create()

        # Inject it live into its placeholder
        app_window = qml_engine.rootObjects()[0]
        placeholder = app_window.findChild(QObject, "ngl_widget_placeholder")
        widget.setParentItem(placeholder)

        self._ngl_widget = widget
        self._qml_player = app_window.findChild(QObject, "player")
        self._qml_player.timeChanged.connect(self._ngl_widget.set_time)

    def set_scene(self, scene: ngl.Scene):
        self._ngl_widget.set_scene(scene)
        self._qml_player.setProperty("duration", scene.duration)
        self._qml_player.setProperty("framerate", list(scene.framerate))
        self._qml_player.setProperty("aspect", list(scene.aspect_ratio))

    def set_callbacks(self, livectls_changed=None, mouse_down=None, zoom=None, pan=None):
        if livectls_changed:
            self._ngl_widget.livectls_changed.connect(livectls_changed)
        if mouse_down:
            self._qml_player.mouseDown.connect(mouse_down)
        if zoom:
            self._qml_player.zoom.connect(zoom)
        if pan:
            self._qml_player.pan.connect(pan)

    def change_live_data(self, key: str, value):
        self._ngl_widget.livectls_changes[key] = value

    def update(self):
        self._ngl_widget.update()

    def stop(self):
        self._ngl_widget.stop()
