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

import locale
import os
import os.path as op
import platform
from textwrap import dedent

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

    # Force C locale to ensure consistent float parsing
    locale.setlocale(locale.LC_ALL, "C")

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


def uri_to_path(uri: str) -> str:
    path = QUrl(uri).path()  # handle the file:// automatically added by Qt/QML
    if platform.system() == "Windows" and path.startswith("/"):
        path = path[1:]
    return path


_LICENSE_APACHE = "https://github.com/NopeForge/nope.gl/blob/main/LICENSE"
_NOTICE = "https://github.com/NopeForge/nope.gl/blob/main/NOTICE"
_NOPE_FORGE = "https://www.nopeforge.org"
_NOPE_MEDIA = "https://github.com/NopeForge/nope.media"


def get_about_content(app_name: str, extra_text: str = "") -> str:
    about = dedent(
        f"""\
        # About

        The Nope {app_name} is part of the [Nope Forge]({_NOPE_FORGE}) project
        and is licensed under the [Apache License, Version 2.0]({_LICENSE_APACHE}).

        See the [NOTICE]({_NOTICE}) file for more information.

        """
    )
    if extra_text:
        about += extra_text + "\n"
    about += dedent(
        f"""\
        This software also relies on [nope.media]({_NOPE_MEDIA}), which is under
        LGPL v2.1.

        Finally, it depends on the following third-party libraries:
        - [FFmpeg](https://www.ffmpeg.org): LGPL v2.1 / GPL v3
        - [Harfbuzz](https://harfbuzz.github.io): MIT
        - [FreeType](https://freetype.org): FTL / GPL v2
        - [FriBidi](https://github.com/fribidi/fribidi): LGPL v2.1
        - [Qt6](https://www.qt.io): GPL v3
        - [MoltenVK](https://github.com/KhronosGroup/MoltenVK): Apache v2
        - [Pillow](https://python-pillow.org): BSD
        - [python-watchdog](https://github.com/gorakhargosh/watchdog): Apache v2
        - [python-packaging](https://github.com/pypa/packaging): Apache v2
        """
    )
    return about
