#
# Copyright 2023 Clément Bœsch <u pkh.me>
# Copyright 2023 Nope Foundry
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
import platform
import subprocess
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional

from pynopegl_utils import qml
from pynopegl_utils.com import query_scene
from pynopegl_utils.misc import SceneCfg, SceneInfo, get_viewport
from pynopegl_utils.qml import livectls
from pynopegl_utils.scriptsmgr import ScriptsManager
from pynopegl_utils.viewer.config import ENCODE_PROFILES, RESOLUTIONS, Config
from PySide6.QtCore import QAbstractListModel, QModelIndex, QObject, Qt, QUrl, Slot
from PySide6.QtGui import QColor

import pynopegl as ngl


class _Viewer:
    def __init__(self, app, qml_engine, ngl_widget, args):
        self._app = app
        self._qml_engine = qml_engine
        app_window = qml_engine.rootObjects()[0]
        app_window.closing.connect(self._close)

        self._cancel_export_request = False
        self._scripts_mgr = None
        self._scene_data = []
        self._current_scene_data = None

        # Needed so that all the objects found with fincChild() remains accessible
        # https://bugreports.qt.io/browse/QTBUG-116311
        self._window = app_window

        self._config = Config()
        choices = self._config.CHOICES

        script = self._config.get("script")
        scene = self._config.get("scene")
        assert script is not None
        assert scene is not None
        if len(args) > 1:
            script = args[1]
            if len(args) > 2:
                scene = args[2]
        self._config.set_script(script)
        self._config.set_scene(scene)

        app_window.exportVideo.connect(self._export_video)
        app_window.cancelExport.connect(self._cancel_export)

        self._params_model = UIElementsModel()

        # XXX should we have a trigger/apply button for those?
        self._params_model.dataChanged.connect(self._params_data_changed)

        self._params_listview = app_window.findChild(QObject, "paramList")
        self._params_listview.setProperty("model", self._params_model)

        self._livectls_model = UIElementsModel()
        self._livectls_model.dataChanged.connect(self._livectl_data_changed)
        self._livectls_listview = app_window.findChild(QObject, "controlList")
        self._livectls_listview.setProperty("model", self._livectls_model)

        self._ngl_widget = ngl_widget
        self._ngl_widget.livectls_changed.connect(self._livectls_model.reset_data_model)

        self._script = app_window.findChild(QObject, "script")
        self._script.setProperty("text", script)
        self._script.editingFinished.connect(self._select_script)

        # The appropriate scene will be selected once the module is loaded
        self._scene_list = app_window.findChild(QObject, "sceneList")
        self._scene_list.activated.connect(self._select_scene)

        self._player = app_window.findChild(QObject, "player")
        self._player.timeChanged.connect(ngl_widget.set_time)

        self._export_bar = app_window.findChild(QObject, "exportBar")

        framerate_names = ["%.5g FPS" % (fps[0] / fps[1]) for fps in choices["framerate"]]
        framerate = self._config.get("framerate")
        framerate_index = choices["framerate"].index(framerate)
        framerate_list = app_window.findChild(QObject, "framerateList")
        framerate_list.setProperty("model", framerate_names)
        framerate_list.setProperty("currentIndex", framerate_index)
        framerate_list.activated.connect(self._select_framerate)

        self._script_dialog = app_window.findChild(QObject, "scriptDialog")
        self._export_dialog = app_window.findChild(QObject, "exportDialog")

        export_filename = self._config.get("export_filename")
        self._export_filename_text = app_window.findChild(QObject, "exportFile")
        self._export_filename_text.setProperty("text", export_filename)
        self._export_filename_text.editingFinished.connect(self._set_export_filename)
        self._set_export_filename()  # make sure the current folder is set for the initial value

        export_res_names = choices["export_res"]
        export_res = self._config.get("export_res")
        export_res_index = choices["export_res"].index(export_res)
        export_res_list = app_window.findChild(QObject, "resList")
        export_res_list.setProperty("model", export_res_names)
        export_res_list.setProperty("currentIndex", export_res_index)
        export_res_list.activated.connect(self._select_export_res)

        export_profile_names = [ENCODE_PROFILES[p].name for p in choices["export_profile"]]
        export_profile = self._config.get("export_profile")
        export_profile_index = choices["export_profile"].index(export_profile)
        export_profile_list = app_window.findChild(QObject, "profileList")
        export_profile_list.setProperty("model", export_profile_names)
        export_profile_list.setProperty("currentIndex", export_profile_index)
        export_profile_list.activated.connect(self._select_export_profile)

        export_samples_names = ["Disabled" if s == 0 else f"x{s}" for s in choices["export_samples"]]
        export_samples = self._config.get("export_samples")
        export_samples_index = choices["export_samples"].index(export_samples)
        export_samples_list = app_window.findChild(QObject, "samplesList")
        export_samples_list.setProperty("model", export_samples_names)
        export_samples_list.setProperty("currentIndex", export_samples_index)
        export_samples_list.activated.connect(self._select_export_samples)

        self._errorText = app_window.findChild(QObject, "errorText")

        try:
            ret = subprocess.run(
                ["ffmpeg", "-version"],
                check=True,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
        except Exception:
            app_window.disable_export("No working `ffmpeg` command found,\nexport is disabled.")

        self._select_script()

    @Slot()
    def _close(self):
        self._cancel_export()

    @Slot()
    def _select_script(self):
        script = self._script.property("text")
        script = QUrl(script).path()  # handle the file:// automatically added by Qt/QML

        if script.endswith(".py"):
            if platform.system() == "Windows" and script.startswith("/"):
                script = script[1:]
            dirname = Path(script).resolve().parent.as_posix()
            dirname = QUrl.fromLocalFile(dirname).url()
            self._script_dialog.setProperty("currentFolder", dirname)

        self._config.set_script(script)

        if not script:
            return

        if self._scripts_mgr is not None:
            self._scripts_mgr.pause()
            del self._scripts_mgr

        self._scripts_mgr = ScriptsManager(script)
        self._scripts_mgr.error.connect(self._set_error)
        self._scripts_mgr.scriptsChanged.connect(self._scriptmgr_scripts_changed)
        self._scripts_mgr.start()

    @Slot(int)
    def _select_scene(self, index):
        if index < 0 or index >= len(self._scene_data):
            self._ngl_widget.stop()
            return

        scene_data = self._scene_data[index]
        self._current_scene_data = scene_data
        self._config.set_scene(scene_data["scene_id"])

        self._set_widgets_from_specs(scene_data["func"].widgets_specs)

        self._load_current_scene()

    @Slot(int)
    def _select_framerate(self, index: int):
        framerate = self._config.CHOICES["framerate"][index]
        self._config.set_framerate(framerate)
        self._load_current_scene()

    @Slot()
    def _set_export_filename(self):
        filename = self._export_filename_text.property("text")
        filename = QUrl(filename).path()  # handle the file:// automatically added by Qt/QML

        if platform.system() == "Windows" and filename.startswith("/"):
            filename = filename[1:]
        dirname = Path(filename).resolve().parent.as_posix()
        dirname = QUrl.fromLocalFile(dirname).url()
        self._export_dialog.setProperty("currentFolder", dirname)

        self._config.set_export_filename(filename)

    @Slot(int)
    def _select_export_res(self, index: int):
        res = self._config.CHOICES["export_res"][index]
        self._config.set_export_res(res)

    @Slot(int)
    def _select_export_profile(self, index: int):
        profile = self._config.CHOICES["export_profile"][index]
        self._config.set_export_profile(profile)

    @Slot(int)
    def _select_export_samples(self, index: int):
        samples = self._config.CHOICES["export_samples"][index]
        self._config.set_export_samples(samples)

    @Slot()
    def _cancel_export(self):
        self._cancel_export_request = True

    @Slot(str, int, int, int)
    def _export_video(self, filename: str, res_index: int, profile_index: int, samples: int):
        scene_data = self._current_scene_data
        if scene_data is None:
            return

        res = RESOLUTIONS[self._config.CHOICES["export_res"][res_index]]
        profile = ENCODE_PROFILES[self._config.CHOICES["export_profile"][profile_index]]

        extra_args = self._get_scene_building_extra_args()

        # XXX: reduce the scope of the try-except?
        try:
            scene_info: SceneInfo = scene_data["func"](**extra_args)
            scene = scene_info.scene

            # Apply user live changes
            model_data = livectls.get_model_data(scene)
            changes = []
            for entry in model_data:
                value = scene_data["live"].get(entry["label"])
                if value is None:
                    continue
                entry["val"] = value
                changes.append(entry)
            livectls.apply_changes(changes)

            cfg = self._get_scene_cfg()
            cfg.samples = samples

            ar = scene.aspect_ratio
            height = res
            width = int(height * ar[0] / ar[1])

            # make sure it's a multiple of 2 for the h264 codec
            width &= ~1

            fps = scene.framerate
            duration = scene.duration

            fd_r, fd_w = os.pipe()

            # fmt: off
            cmd = [
                "ffmpeg", "-r", "%d/%d" % fps,
                "-v", "warning",
                "-nostats", "-nostdin",
                "-f", "rawvideo",
                "-video_size", "%dx%d" % (width, height),
                "-pixel_format", "rgba",
                "-i", "pipe:%d" % fd_r,
            ] + profile.args + [
                "-f", profile.format,
                "-y", filename,
            ]
            # fmt: on

            reader = subprocess.Popen(cmd, pass_fds=(fd_r,))
            os.close(fd_r)

            capture_buffer = bytearray(width * height * 4)

            ctx = ngl.Context()
            ctx.configure(
                ngl.Config(
                    platform=ngl.Platform.AUTO,
                    backend=ngl.Backend.AUTO,
                    offscreen=True,
                    width=width,
                    height=height,
                    viewport=get_viewport(width, height, scene.aspect_ratio),
                    samples=cfg.samples,
                    capture_buffer=capture_buffer,
                )
            )
            ctx.set_scene(scene)

            # Draw every frame
            nb_frame = int(duration * fps[0] / fps[1])
            for i in range(nb_frame):
                if self._cancel_export_request:
                    break
                time = i * fps[1] / float(fps[0])
                ctx.draw(time)
                os.write(fd_w, capture_buffer)
                progress = i / (nb_frame - 1)
                self._export_bar.setProperty("value", progress)
                self._app.processEvents()  # refresh UI

            if self._cancel_export_request:
                self._cancel_export_request = False
            else:
                self._window.finish_export()

            os.close(fd_w)
            reader.wait()

            del ctx

            self._ngl_widget.set_scene(scene)

        except Exception as e:
            self._window.abort_export(str(e))

    @Slot(str)
    def _set_error(self, error: Optional[str]):
        if error is None:
            error = ""

        self._errorText.setProperty("text", error)

        # Useful to have it in the console as well
        if error:
            print(error)

    @Slot(list)
    def _scriptmgr_scripts_changed(self, scenes):
        cur_scene_id = None
        scene_data = self._current_scene_data
        if scene_data:
            cur_scene_id = scene_data["scene_id"]
        else:
            cur_scene_id = self._config.get("scene")

        scene_data = []
        for module_name, sub_scenes in scenes:
            base = module_name.rsplit(".", 1)[-1]
            prefix = base + "." if len(scenes) > 1 else ""
            for scene_name, func in sub_scenes:
                scene_data.append(dict(scene_id=prefix + scene_name, module_name=module_name, func=func, live={}))

        self._scene_data = scene_data

        scene_ids = [data["scene_id"] for data in scene_data]
        self._scene_list.setProperty("model", scene_ids)

        index = scene_ids.index(cur_scene_id) if cur_scene_id and cur_scene_id in scene_ids else 0
        self._scene_list.setProperty("currentIndex", index)
        self._select_scene(index)

    def _get_scene_cfg(self):
        return SceneCfg(
            framerate=self._config.get("framerate"),
        )

    def _get_scene_building_extra_args(self) -> Dict[str, Any]:
        extra_args = {}
        for i in range(self._params_model.rowCount()):
            data = self._params_model.get_row(i)

            val = data["val"]
            # It appears that the QML cannot modify the model entries with
            # native backend types, so we have to convert them back after the
            # model data has been tainted by the QML
            if data["type"] == "vector" and not isinstance(val, (list, tuple)):
                # QJSValue array to native list
                val = [val.property(i).toNumber() for i in range(val.property("length").toInt())]
            elif data["type"] == "color":
                val = QColor.getRgbF(val)[:3]
            elif data["type"] == "file" and val is not None:
                val = QUrl(val).path()  # handle the file:// automatically added by Qt/QML
            extra_args[data["label"]] = val

        return extra_args

    # Convert from scene namedtuple to role value in the QML model
    _WIDGET_TYPES_MODEL_MAP = dict(
        Range="range",
        Vector="vector",
        Color="color",
        Bool="bool",
        File="file",
        List="list",
        Text="text",
    )

    def _set_widgets_from_specs(self, widgets_specs):
        scene_data = self._current_scene_data
        assert scene_data is not None

        widgets_specs = scene_data["func"].widgets_specs

        model_data = []
        for key, default, ctl_id, ctl_data in widgets_specs:
            type_ = self._WIDGET_TYPES_MODEL_MAP.get(ctl_id)
            if type_ is None:
                print(f"widget type {ctl_id} is not yet supported")
                continue

            data = dict(type=type_, label=key, val=default)
            if ctl_id == "Range":
                data["min"] = ctl_data["range"][0]
                data["max"] = ctl_data["range"][1]
                data["step"] = 1 / ctl_data["unit_base"]
            elif ctl_id == "Vector":
                n = ctl_data["n"]
                minv = ctl_data.get("minv")
                maxv = ctl_data.get("maxv")
                data["n"] = n
                data["min"] = [0] * n if minv is None else list(minv)
                data["val"] = list(default)
                data["max"] = [1] * n if maxv is None else list(maxv)
            elif ctl_id == "Color":
                data["val"] = QColor.fromRgbF(*default)
            elif ctl_id == "List":
                data["choices"] = list(ctl_data["choices"])
            elif ctl_id == "File":
                data["filter"] = ctl_data["filter"]

            model_data.append(data)

        self._params_model.reset_data_model(model_data)

    def _load_current_scene(self):
        scene_data = self._current_scene_data
        if not scene_data:
            return

        assert self._scripts_mgr is not None  # module is always loaded if we have the current scene data set

        cfg = self._get_scene_cfg()

        extra_args = self._get_scene_building_extra_args()

        self._scripts_mgr.inc_query_count()
        self._scripts_mgr.pause()
        query_info = query_scene(scene_data["module_name"], scene_data["func"], cfg, extra_args)
        self._scripts_mgr.update_filelist(query_info.filelist)
        self._scripts_mgr.update_modulelist(query_info.modulelist)
        self._scripts_mgr.resume()
        self._scripts_mgr.dec_query_count()

        self._set_error(query_info.error)
        if query_info.error:
            return

        scene_info: SceneInfo = query_info.ret
        scene = scene_info.scene

        # TODO Ideally we would honor the saved live data settings for the
        # scene, but this implies an update of all the widgets data so we reset
        # it instead.
        scene_data["live"] = {}

        self._ngl_widget.set_scene(scene)

        self._player.setProperty("duration", scene.duration)
        self._player.setProperty("framerate", list(scene.framerate))
        self._player.setProperty("aspect", list(scene.aspect_ratio))

    @Slot()
    def _params_data_changed(self, top_left, bottom_right):
        self._load_current_scene()

    @Slot()
    def _livectl_data_changed(self, top_left, bottom_right):
        scene_data = self._current_scene_data
        assert scene_data is not None

        for row in range(top_left.row(), bottom_right.row() + 1):
            data = self._livectls_model.get_row(row)
            self._ngl_widget.livectls_changes[data["label"]] = data

            # We do not store the associated node because we want to keep this
            # data context agnostic
            scene_data["live"][data["label"]] = data["val"]
        self._ngl_widget.update()


class UIElementsModel(QAbstractListModel):
    """Model for both the build scene options widgets and live controls"""

    _roles = ["type", "label", "val", "min", "max", "n", "step", "choices", "filter"]
    _roles_map = {Qt.UserRole + i: s for i, s in enumerate(_roles)}

    def __init__(self, parent=None):
        super().__init__(parent)
        self._data = []

    @Slot(object)
    def reset_data_model(self, data):
        self.beginResetModel()
        self._data = data
        self.endResetModel()

    def get_row(self, row):
        return self._data[row]

    def roleNames(self):
        names = super().roleNames()
        names.update({k: v.encode() for k, v in self._roles_map.items()})
        return names

    def rowCount(self, parent=QModelIndex()):
        return len(self._data)

    def data(self, index, role: int):
        if not index.isValid():
            return None
        return self._data[index.row()].get(self._roles_map[role])

    def setData(self, index, value, role):
        if not index.isValid():
            return False
        item = self._data[index.row()]
        item[self._roles_map[role]] = value
        self.dataChanged.emit(index, index)
        return True


def run():
    qml_file = (Path(qml.__file__).resolve().parent / "viewer.qml").as_posix()
    app, engine = qml.create_app_engine(sys.argv, qml_file)
    ngl_widget = qml.create_ngl_widget(engine)
    viewer = _Viewer(app, engine, ngl_widget, sys.argv)
    ret = app.exec()
    del viewer
    sys.exit(ret)
