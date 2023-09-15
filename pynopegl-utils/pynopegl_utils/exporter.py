#
# Copyright 2017-2022 GoPro Inc.
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

from typing import Callable, Optional

from pynopegl_utils.com import query_scene
from pynopegl_utils.export import export_workers
from pynopegl_utils.misc import SceneCfg, SceneInfo
from PySide6 import QtCore, QtGui


class Exporter(QtCore.QThread):
    progressed = QtCore.Signal(int)
    failed = QtCore.Signal(str)
    export_finished = QtCore.Signal()

    def __init__(
        self,
        get_scene_info: Callable[..., Optional[SceneInfo]],
        filename,
        res_id: str,
        profile_id: str,
    ):
        super().__init__()
        self._get_scene_info = get_scene_info
        self._filename = filename
        self._res_id = res_id
        self._profile_id = profile_id
        self._cancelled = False

    def run(self):
        filename, res_id, profile_id = self._filename, self._res_id, self._profile_id

        try:
            scene_info = self._get_scene_info()
            if not scene_info:
                self.failed.emit("You didn't select any scene to export.")
                return

            export = export_workers(scene_info, filename, res_id, profile_id)
            for progress in export:
                self.progressed.emit(progress)
                if self._cancelled:
                    break
            self.export_finished.emit()
        except Exception as e:
            self.failed.emit("Something went wrong while trying to encode, check encoding parameters")

    def cancel(self):
        self._cancelled = True


def test_export():
    import sys

    def _get_scene(**cfg_overrides):
        from pynopegl_utils.examples.misc import triangle

        cfg = SceneCfg(duration=5, **cfg_overrides)
        query_info = query_scene("pynopegl_utils.examples", triangle, cfg)
        if query_info.error is not None:
            print(query_info.error)
            return None
        return query_info.ret

    def print_progress(progress):
        sys.stdout.write("\r%d%%" % progress)
        sys.stdout.flush()
        if progress == 100:
            sys.stdout.write("\n")

    if len(sys.argv) != 2:
        print("Usage: %s <outfile>" % sys.argv[0])
        sys.exit(0)

    filename = sys.argv[1]
    app = QtGui.QGuiApplication(sys.argv)

    exporter = Exporter(_get_scene, filename, "240p", "mp4_h264_420")
    exporter.progressed.connect(print_progress)
    exporter.start()
    exporter.wait()

    app.exit()


if __name__ == "__main__":
    test_export()
