#
# Copyright 2018-2022 GoPro Inc.
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

import hashlib
import os
import os.path as op
import tempfile
import time

from pynopegl_utils.module import load_script
from PySide6 import QtCore


class _HooksCaller:
    _HOOKS = ("get_session_info", "get_sessions", "scene_change", "sync_file")

    def __init__(self, hooks_script):
        self._module = load_script(hooks_script)
        if not all(hasattr(self._module, hook) for hook in self._HOOKS):
            raise NotImplementedError(f"{hooks_script}: the following functions must be implemented: {self._HOOKS}")

    def _has_hook(self, name):
        return hasattr(self._module, name)

    def get_session_info(self, session_id):
        return self._module.get_session_info(session_id)

    def get_sessions(self):
        return self._module.get_sessions()

    @staticmethod
    def _uint_clear_color(vec4_color):
        uint_color = 0
        for i, comp in enumerate(vec4_color):
            comp_val = int(round(comp * 0xFF)) & 0xFF
            uint_color |= comp_val << (24 - i * 8)
        return uint_color

    def scene_change(self, session_id, local_scene, cfg):
        return self._module.scene_change(
            session_id,
            local_scene,
            cfg["duration"],
            cfg["aspect_ratio"],
            cfg["framerate"],
            self._uint_clear_color(cfg["clear_color"]),
            cfg["samples"],
        )

    @staticmethod
    def _hash_filename(filename):
        statinfo = os.stat(filename)
        sha256 = hashlib.sha256()
        sha256.update(filename.encode())
        sha256.update(str(statinfo.st_size).encode())
        sha256.update(str(statinfo.st_mtime).encode())
        digest = sha256.hexdigest()
        _, ext = op.splitext(filename)
        return op.join(digest + ext)

    def sync_file(self, session_id, localfile):
        return self._module.sync_file(session_id, localfile, self._hash_filename(localfile))


class HooksCaller:
    def __init__(self, hooks_scripts):
        self._callers = [_HooksCaller(hooks_script) for hooks_script in hooks_scripts]

    def _get_caller_session_id(self, unique_session_id):
        istr, session_id = unique_session_id.split(":", maxsplit=1)
        return self._callers[int(istr)], session_id

    def get_sessions(self):
        """
        Session IDs may be identical accross hook systems. A pathological case
        is with several instances of the same hook system, but it could
        happen in other situations as well since hook systems don't know each
        others and may pick common identifiers), so we need to make them
        unique.

        This methods makes these session IDs unique by adding the index of
        the caller as prefix.
        """
        sessions = {}
        for caller_id, caller in enumerate(self._callers):
            for session_id, session_desc in caller.get_sessions():
                sid = f"{caller_id}:{session_id}"
                sessions[sid] = dict(sid=sid, desc=session_desc)
        return sessions

    def get_session_info(self, session_id):
        caller, session_id = self._get_caller_session_id(session_id)
        return caller.get_session_info(session_id)

    def scene_change(self, session_id, local_scene, cfg):
        caller, session_id = self._get_caller_session_id(session_id)
        return caller.scene_change(session_id, local_scene, cfg)

    def sync_file(self, session_id, localfile):
        caller, session_id = self._get_caller_session_id(session_id)
        return caller.sync_file(session_id, localfile)


class _SessionInfoWorker(QtCore.QObject):
    _process = QtCore.Signal()
    success = QtCore.Signal(object, object)
    error = QtCore.Signal(object)

    def __init__(self, hooks_caller, session):
        super().__init__()
        self._hooks_caller = hooks_caller
        self._session = session
        self._lock = QtCore.QMutex()
        self._submit = False
        self._process.connect(self._run)

    def submit_session_info(self):
        with QtCore.QMutexLocker(self._lock):
            self._submit = True
        self._process.emit()

    @QtCore.Slot()
    def _run(self):
        submit = False
        with QtCore.QMutexLocker(self._lock):
            submit = self._submit
            self._submit = False
        if not submit:
            return
        try:
            info = self._hooks_caller.get_session_info(self._session["sid"])
        except Exception as e:
            print(f"Could not get information for session '{self._session['sid']}': {e}")
            self.error.emit(self._session)
            return
        self.success.emit(self._session, info)


class _SceneChangeWorker(QtCore.QObject):
    _process = QtCore.Signal()
    uploadingFile = QtCore.Signal(str, int, int, str)
    buildingScene = QtCore.Signal(str, str, str)
    sendingScene = QtCore.Signal(str, str)
    success = QtCore.Signal(str, str, float)
    error = QtCore.Signal(str, str)

    def __init__(self, get_scene_func, hooks_caller):
        super().__init__()
        self._get_scene_func = get_scene_func
        self._hooks_caller = hooks_caller
        self._lock = QtCore.QMutex()
        self._scene_change = None
        self._process.connect(self._run)

    def submit_scene_change(self, scene_id, session):
        with QtCore.QMutexLocker(self._lock):
            self._scene_change = (scene_id, session)
        self._process.emit()

    @staticmethod
    def _filename_escape(filename):
        s = ""
        for cval in filename.encode("utf-8"):
            if cval >= ord("!") and cval <= ord("~") and cval != ord("%"):
                s += chr(cval)
            else:
                s += "%%%02x" % (cval & 0xFF)
        return s

    @QtCore.Slot()
    def _run(self):
        scene_change = None
        with QtCore.QMutexLocker(self._lock):
            scene_change = self._scene_change
            self._scene_change = None
        if scene_change is None:
            return

        start_time = time.time()
        scene_id, session = scene_change
        session_id = session["sid"]
        backend = session["backend"]
        system = session["system"]

        self.buildingScene.emit(session_id, backend, system)
        cfg = self._get_scene_func(backend=backend, system=system)
        if not cfg:
            self.error.emit(session_id, "Error getting scene")
            return

        try:
            # The serialized scene is associated with a bunch of assets which we
            # need to sync. Similarly, the remote assets directory might be
            # different from the one in local, so we need to fix up the scene
            # appropriately.
            serialized_scene = cfg["scene"].decode("ascii")
            filelist = [m["filename"] for m in cfg["medias"]] + cfg["files"]
            for i, localfile in enumerate(filelist, 1):
                self.uploadingFile.emit(session_id, i, len(filelist), localfile)
                try:
                    remotefile = self._hooks_caller.sync_file(session_id, localfile)
                except Exception as e:
                    self.error.emit(session_id, "Error while uploading %s: %s" % (localfile, str(e)))
                    return
                serialized_scene = serialized_scene.replace(
                    self._filename_escape(localfile), self._filename_escape(remotefile)
                )

            # The serialized scene is then stored in a file which is then
            # communicated with additional parameters to the user
            # tempfile.NamedTemporaryFile has limitations on Windows, see
            # https://docs.python.org/3/library/tempfile.html#tempfile.NamedTemporaryFile
            with tempfile.TemporaryDirectory(prefix="ngl_") as td:
                fname = op.join(td, "scene.ngl")
                with open(fname, "w", newline="\n") as fp:
                    fp.write(serialized_scene)
                self.sendingScene.emit(session_id, scene_id)
                try:
                    self._hooks_caller.scene_change(session_id, fname, cfg)
                except Exception as e:
                    self.error.emit(session_id, "Error while sending scene: %s" % str(e))
                    return
                self.success.emit(session_id, scene_id, time.time() - start_time)
        except Exception as e:
            self.error.emit(session_id, "Error: %s" % str(e))


class HooksController(QtCore.QObject):
    session_added = QtCore.Signal(object)
    session_removed = QtCore.Signal(str)
    session_info_changed = QtCore.Signal(object)

    def __init__(self, get_scene_func, hooks_caller):
        super().__init__()
        self._get_scene_func = get_scene_func
        self._hooks_caller = hooks_caller
        self._threads = {}
        self._session_info_workers = {}
        self._scene_change_workers = {}
        self._sessions_cache = {}
        self._sessions_state = {}

    def stop_threads(self):
        for thread in self._threads.values():
            thread.exit()
            thread.wait()
        self._threads = {}

    def refresh_sessions(self):
        sessions = self._hooks_caller.get_sessions()

        removed_sessions = [sid for sid in self._sessions_cache.keys() if sid not in sessions]
        for session_id in removed_sessions:
            del self._sessions_cache[session_id]
            self.session_removed.emit(session_id)

        for session_id, session in sessions.items():
            if session_id not in self._threads:
                thread = QtCore.QThread()
                thread.start()
                self._threads[session_id] = thread

                worker = _SessionInfoWorker(self._hooks_caller, session)
                worker.success.connect(self._session_info_success)
                worker.error.connect(self._session_info_error)
                worker.moveToThread(thread)
                self._session_info_workers[session_id] = worker

                worker = _SceneChangeWorker(self._get_scene_func, self._hooks_caller)
                worker.uploadingFile.connect(self._hooks_uploading)
                worker.buildingScene.connect(self._hooks_building_scene)
                worker.sendingScene.connect(self._hooks_sending_scene)
                worker.success.connect(self._hooks_success)
                worker.error.connect(self._hooks_error)
                worker.moveToThread(thread)
                self._scene_change_workers[session_id] = worker

            worker = self._session_info_workers[session_id]
            worker.submit_session_info()

    def enable_session(self, session_id, enabled):
        if session_id in self._sessions_cache:
            self._sessions_cache[session_id]["enabled"] = enabled
            self._sessions_state[session_id] = enabled

    @QtCore.Slot(object, str, object)
    def _session_info_success(self, session, info):
        backend = info.get("backend")
        system = info.get("system")
        cached_session = self._sessions_cache.get(session["sid"])
        if cached_session is None:
            enabled = self._sessions_state.get(session["sid"], True)
            new_session = dict(
                sid=session["sid"],
                desc=session["desc"],
                backend=backend,
                system=system,
                enabled=enabled,
                status="",
            )
            self._sessions_cache[new_session["sid"]] = new_session
            self.session_added.emit(new_session)
        elif (
            session["desc"] != cached_session["desc"]
            or backend != cached_session["backend"]
            or system != cached_session["system"]
        ):
            cached_session["desc"] = session["desc"]
            cached_session["backend"] = backend
            cached_session["system"] = system
            self.session_info_changed.emit(cached_session)

    @QtCore.Slot(object, str)
    def _session_info_error(self, session):
        session_id = session["sid"]
        if session_id in self._sessions_cache:
            del self._sessions_cache[session_id]
            self.session_removed.emit(session_id)

    def process(self, module_name, scene_name):
        for session in self._sessions_cache.values():
            if not session["enabled"]:
                continue
            scene_id = f"{module_name}.{scene_name}"
            worker = self._scene_change_workers[session["sid"]]
            worker.submit_scene_change(scene_id, session)

    @QtCore.Slot(str, int, int, str)
    def _hooks_uploading(self, session_id, i, n, filename):
        session = self._sessions_cache.get(session_id, None)
        if not session:
            return
        session["status"] = "Uploading [%d/%d]: %s..." % (i, n, filename)
        self.session_info_changed.emit(session)

    @QtCore.Slot(str, str, str)
    def _hooks_building_scene(self, session_id, backend, system):
        session = self._sessions_cache.get(session_id, None)
        if not session:
            return
        session["status"] = f"Building {system} scene in {backend}..."
        self.session_info_changed.emit(session)

    @QtCore.Slot(str, str)
    def _hooks_sending_scene(self, session_id, scene):
        session = self._sessions_cache.get(session_id, None)
        if not session:
            return
        session["status"] = "Sending scene %s..." % scene
        self.session_info_changed.emit(session)

    @QtCore.Slot(str, str, float)
    def _hooks_success(self, session_id, scene, t):
        session = self._sessions_cache.get(session_id, None)
        if not session:
            return
        session["status"] = f"Submitted {scene} in {t:f}"
        self.session_info_changed.emit(session)

    @QtCore.Slot(str, str)
    def _hooks_error(self, session_id, error):
        session = self._sessions_cache.get(session_id, None)
        if not session:
            return
        session["status"] = error
        self.session_info_changed.emit(session)
