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

import json
import os
import os.path as op
from pathlib import Path
from typing import Any, Dict, Optional, Tuple

from pynopegl_utils.export import ENCODE_PROFILES, RESOLUTIONS
from PySide6 import QtCore


class Config(QtCore.QObject):
    FILEPATH = (
        Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local" / "share")) / "nope.gl" / "viewer.json"
    ).as_posix()
    CHOICES = {
        "framerate": [
            (8, 1),
            (12, 1),
            (15, 1),
            (24000, 1001),
            (24, 1),
            (25, 1),
            (30000, 1001),
            (30, 1),
            (50, 1),
            (60000, 1001),
            (60, 1),
        ],
        "export_res": list(RESOLUTIONS.keys()),
        "export_profile": list(ENCODE_PROFILES.keys()),
        "export_samples": [0, 2, 4, 8],
    }

    def __init__(self):
        super().__init__()

        self._cfg = {
            # Script
            "script": "pynopegl_utils.viewer.scenes",
            "scene": "demo",
            # Rendering settings
            "framerate": (60, 1),
            # Export
            "export_filename": (Path.home() / "nope.mp4").as_posix(),
            "export_res": "1080p",
            "export_profile": "mp4_h264_420",
            "export_samples": 0,
        }

        self._needs_saving = False

        if op.exists(self.FILEPATH):
            with open(self.FILEPATH) as f:
                self._cfg.update(self._sanitized_config(json.load(f)))
        else:
            self._needs_saving = True

        self._config_timer = QtCore.QTimer()
        self._config_timer.setInterval(1000)  # every second
        self._config_timer.timeout.connect(self._check_config)
        self._config_timer.start()

    def _sanitized_config(self, cfg):
        out_cfg = {}
        for key, value in cfg.items():
            allowed_values = self.CHOICES.get(key)
            if isinstance(value, list):
                value = tuple(value)
            if allowed_values is None or value in allowed_values:
                out_cfg[key] = value
            else:
                print(f"warning: {value} not allowed for {key}")
        return out_cfg

    @QtCore.Slot()
    def _check_config(self):
        if not self._needs_saving:
            return

        config_dir = op.dirname(self.FILEPATH)
        os.makedirs(config_dir, exist_ok=True)

        config_file = open(self.FILEPATH, "w")
        json.dump(self._cfg, config_file, indent=4)
        config_file.close()

        self._needs_saving = False

    def get(self, key):
        ret = self._cfg.get(key)
        return tuple(ret) if isinstance(ret, list) else ret

    def _set_cfg(self, key, value):
        if self._cfg.get(key) == value:
            return
        self._cfg[key] = value
        self._needs_saving = True

    def set_script(self, script: str):
        self._set_cfg("script", script)

    def set_scene(self, scene: str):
        self._set_cfg("scene", scene)

    def set_framerate(self, fr: Tuple[int, int]):
        self._set_cfg("framerate", fr)

    def set_export_filename(self, filename: str):
        self._set_cfg("export_filename", filename)

    def set_export_res(self, res: int):
        self._set_cfg("export_res", res)

    def set_export_profile(self, profile: str):
        self._set_cfg("export_profile", profile)

    def set_export_samples(self, samples: int):
        self._set_cfg("export_samples", samples)
