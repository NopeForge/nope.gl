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

import os
import os.path as op
import platform

from pynopegl_utils.qml import ngl_widget  # noqa: register NopeGLWidget as a QML element
from PySide6.QtCore import QObject, QUrl
from PySide6.QtGui import QGuiApplication, QSurfaceFormat
from PySide6.QtQml import QQmlApplicationEngine, QQmlComponent
from PySide6.QtQuick import QQuickWindow, QSGRendererInterface


def create_app_engine(argv, qml_file):
    # We only support EGL but Qt still uses the legacy GLX by default
    os.environ["QT_XCB_GL_INTEGRATION"] = "xcb_egl"

    # We do not support Opengl 2.1 contexts (the default on macOS), so we
    # force OpenGL 4.1 (Core Profile)
    if platform.system() == "Darwin":
        surface_format = QSurfaceFormat()
        surface_format.setRenderableType(QSurfaceFormat.OpenGL)
        major_version, minor_version = (4, 1)
        surface_format.setVersion(major_version, minor_version)
        surface_format.setProfile(QSurfaceFormat.CoreProfile)
        surface_format.setDepthBufferSize(24)
        surface_format.setStencilBufferSize(8)
        surface_format.setAlphaBufferSize(8)
        QSurfaceFormat.setDefaultFormat(surface_format)

    app = QGuiApplication(argv)
    QQuickWindow.setGraphicsApi(QSGRendererInterface.OpenGL)
    engine = QQmlApplicationEngine(qml_file)
    engine.quit.connect(app.quit)
    return app, engine


def create_ngl_widget(engine):
    # Dynamically create an instance of the rendering widget
    widget_url = QUrl.fromLocalFile(op.join(op.dirname(__file__), "NopeGLWidget.qml"))
    component = QQmlComponent(engine)
    component.loadUrl(widget_url)
    widget = component.create()

    # Inject it live into its placeholder
    app_window = engine.rootObjects()[0]
    placeholder = app_window.findChild(QObject, "ngl_widget_placeholder")
    widget.setParentItem(placeholder)

    return widget
