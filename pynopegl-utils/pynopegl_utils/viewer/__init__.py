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

import platform
import subprocess
import sys
from fractions import Fraction
from pathlib import Path
from textwrap import dedent
from typing import Any, Dict, List, Optional

from pynopegl_utils import qml
from pynopegl_utils.com import query_scene
from pynopegl_utils.export import export_workers
from pynopegl_utils.misc import SceneCfg, SceneInfo
from pynopegl_utils.qml import livectls
from pynopegl_utils.scriptsmgr import ScriptsManager
from pynopegl_utils.viewer.config import ENCODE_PROFILES, Config
from PySide6.QtCore import QAbstractListModel, QModelIndex, QObject, Qt, QUrl, Slot
from PySide6.QtGui import QColor

import pynopegl as ngl

_LICENSE_APACHE = "https://github.com/NopeFoundry/nope.gl/blob/main/LICENSE"
_NOTICE = "https://github.com/NopeFoundry/nope.gl/blob/main/NOTICE"
_NOPE_FOUNDRY = "https://www.nope-foundry.org"
_NOPE_MEDIA = "https://github.com/NopeFoundry/nope.media"
_ABOUT_POPUP_CONTENT = dedent(
    f"""\
    # About

    The Nope viewer is part of the [Nope Foundry]({_NOPE_FOUNDRY}) project and
    is licensed under the [Apache License, Version 2.0]({_LICENSE_APACHE}).

    See the [NOTICE]({_NOTICE}) file for more information.

    What you create with this software is your sole property. All your artwork
    is free for you to use as you like. That means that the Nope Viewer can be
    used commercially, for any purpose. There are no restrictions whatsoever.

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
        if script.endswith(".py"):
            script = QUrl.fromLocalFile(script).url()
        self._config.set_script(script)
        self._config.set_scene(scene)

        app_window.exportVideo.connect(self._export_video)
        app_window.cancelExport.connect(self._cancel_export)
        app_window.selectScript.connect(self._select_script)
        app_window.selectScene.connect(self._select_scene)
        app_window.selectFramerate.connect(self._select_framerate)
        app_window.selectExportFile.connect(self._select_export_file)
        app_window.selectExportResolution.connect(self._select_export_res)
        app_window.selectExportProfile.connect(self._select_export_profile)
        app_window.selectExportSamples.connect(self._select_export_samples)

        self._params_model = UIElementsModel()

        # XXX should we have a trigger/apply button for those?
        self._params_model.dataChanged.connect(self._params_data_changed)

        self._livectls_model = UIElementsModel()
        self._livectls_model.dataChanged.connect(self._livectl_data_changed)

        self._ngl_widget = ngl_widget
        self._ngl_widget.livectls_changed.connect(self._livectls_model.reset_data_model)

        self._player = app_window.findChild(QObject, "player")
        self._player.timeChanged.connect(ngl_widget.set_time)

        framerate_names = ["%.5g FPS" % (fps[0] / fps[1]) for fps in choices["framerate"]]
        framerate = self._config.get("framerate")
        framerate_index = choices["framerate"].index(framerate)

        extensions = " ".join(sorted(set(f"*.{p.format}" for p in ENCODE_PROFILES.values())))

        export_filename = self._config.get("export_filename")
        export_filename = QUrl.fromLocalFile(export_filename).url()

        export_res_names = choices["export_res"]
        export_res = self._config.get("export_res")
        export_res_index = choices["export_res"].index(export_res)

        export_profile_names = [ENCODE_PROFILES[p].name for p in choices["export_profile"]]
        export_profile = self._config.get("export_profile")
        export_profile_index = choices["export_profile"].index(export_profile)

        export_samples_names = ["Disabled" if s == 0 else f"x{s}" for s in choices["export_samples"]]
        export_samples = self._config.get("export_samples")
        export_samples_index = choices["export_samples"].index(export_samples)

        app_window.set_about_content(_ABOUT_POPUP_CONTENT)

        try:
            ret = subprocess.run(
                ["ffmpeg", "-version"],
                check=True,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
        except Exception:
            app_window.disable_export("No working `ffmpeg` command found,\nexport is disabled.")

        app_window.set_params_model(self._params_model)
        app_window.set_controls_model(self._livectls_model)
        app_window.set_export_name_filters(f"Supported videos ({extensions})")
        app_window.set_export_file(export_filename)
        app_window.set_export_resolutions(export_res_names, export_res_index)
        app_window.set_export_profiles(export_profile_names, export_profile_index)
        app_window.set_export_samples(export_samples_names, export_samples_index)
        app_window.set_script(script)
        app_window.set_framerates(framerate_names, framerate_index)

        self._test_settings()

    @staticmethod
    def _uri_to_path(uri):
        path = QUrl(uri).path()  # handle the file:// automatically added by Qt/QML
        if platform.system() == "Windows" and path.startswith("/"):
            path = path[1:]
        return path

    @Slot()
    def _close(self):
        self._cancel_export()

    @Slot(str)
    def _select_script(self, script: str):
        script = self._uri_to_path(script)

        if script.endswith(".py"):
            script = Path(script).resolve().as_posix()
            dirname = Path(script).parent.as_posix()
            dirname = QUrl.fromLocalFile(dirname).url()
            self._window.set_script_directory(dirname)

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
    def _select_scene(self, index: int):
        if index < 0 or index >= len(self._scene_data):
            self._ngl_widget.stop()
            return

        scene_data = self._scene_data[index]
        self._current_scene_data = scene_data
        self._config.set_scene(scene_data["scene_id"])

        self._set_widgets_from_specs(scene_data["func"].widgets_specs)

        self._load_current_scene()

    def _test_settings(self):
        profile_id = self._config.get("export_profile")
        profile = ENCODE_PROFILES[profile_id]

        # Check if extension is consistent with format
        filename = self._window.get_export_file()
        filename = self._uri_to_path(filename)
        filepath = Path(filename)
        extension = f".{profile.format}"
        if filepath.suffix.lower() != extension:
            new_filename = f"{filepath.stem}{extension}"
            self._window.show_popup(
                "Export file extension is being changed to match requested profile: "
                f"`{filepath.name}` becomes `{new_filename}`."
            )
            filepath = filepath.parent / new_filename
            self._window.set_export_file(filepath.as_posix())

        # Check GIF framerate compatibility
        self._window.hide_export_warning()
        if profile.format == "gif":
            framerate = self._config.get("framerate")
            gif_recommended_framerate = (Fraction(25, 1), Fraction(50, 1))
            if Fraction(*framerate) not in gif_recommended_framerate:
                gif_framerates = ", ".join(f"{x}" for x in gif_recommended_framerate)
                self._window.show_export_warning(
                    f"It is recommended to use one of these\nframe rate when exporting to GIF: {gif_framerates}",
                )

    @Slot(int)
    def _select_framerate(self, index: int):
        framerate = self._config.CHOICES["framerate"][index]
        self._config.set_framerate(framerate)
        self._test_settings()
        self._load_current_scene()

    @Slot(str)
    def _select_export_file(self, export_file: str):
        filename = self._uri_to_path(export_file)

        dirname = Path(filename).resolve().parent.as_posix()
        dirname = QUrl.fromLocalFile(dirname).url()
        self._window.set_export_directory(dirname)

        self._config.set_export_filename(filename)

    @Slot(int)
    def _select_export_res(self, index: int):
        res_id = self._config.CHOICES["export_res"][index]
        self._config.set_export_res(res_id)

    @Slot(int)
    def _select_export_profile(self, index: int):
        profile_id = self._config.CHOICES["export_profile"][index]
        self._config.set_export_profile(profile_id)
        self._test_settings()

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

        filename = self._uri_to_path(filename)

        res_id = self._config.CHOICES["export_res"][res_index]
        profile_id = self._config.CHOICES["export_profile"][profile_index]

        extra_args = self._get_scene_building_extra_args()

        # XXX: reduce the scope of the try-except?
        try:
            cfg = self._get_scene_cfg()
            cfg.samples = samples

            scene_info: SceneInfo = scene_data["func"](cfg, **extra_args)
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

            export = export_workers(scene_info, filename, res_id, profile_id)
            for progress in export:
                if self._cancel_export_request:
                    break
                self._window.set_export_progress(progress / 100)
                self._app.processEvents()  # refresh UI

            if self._cancel_export_request:
                self._cancel_export_request = False
            else:
                self._window.finish_export()

            self._ngl_widget.set_scene(scene)

        except Exception as e:
            self._window.abort_export(str(e))

    @Slot(str)
    def _set_error(self, error: Optional[str]):
        if error is None:
            error = ""

        self._window.set_error(error)

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

        index = scene_ids.index(cur_scene_id) if cur_scene_id and cur_scene_id in scene_ids else 0
        self._window.set_scenes(scene_ids, index)

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
                val = self._uri_to_path(val)
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
