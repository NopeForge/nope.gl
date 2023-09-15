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

from fractions import Fraction
from typing import Callable, Optional

from pynopegl_utils.export import ENCODE_PROFILES, RESOLUTIONS
from pynopegl_utils.exporter import Exporter
from pynopegl_utils.misc import SceneInfo
from PySide6 import QtCore, QtWidgets


class ExportView(QtWidgets.QWidget):
    def __init__(self, get_scene_info: Callable[..., Optional[SceneInfo]], config):
        super().__init__()

        self._config = config

        self._get_scene_info = get_scene_info
        self._framerate = config.get("framerate")
        self._aspect_ratio = config.get("aspect_ratio")

        self._ofile_text = QtWidgets.QLineEdit(config.get("export_filename"))
        ofile_btn = QtWidgets.QPushButton("Browse")

        file_box = QtWidgets.QHBoxLayout()
        file_box.addWidget(self._ofile_text)
        file_box.addWidget(ofile_btn)

        self._res_combobox = QtWidgets.QComboBox()
        all_res = config.CHOICES["export_res"]
        for res in all_res:
            self._res_combobox.addItem(res)
        res_idx = all_res.index(config.get("export_res"))
        self._res_combobox.setCurrentIndex(res_idx)

        self._profile_combobox = QtWidgets.QComboBox()
        all_profile = ENCODE_PROFILES
        for profile_id, profile in all_profile.items():
            self._profile_combobox.addItem(profile.name, userData=profile_id)
        profile_idx = config.CHOICES["export_profile"].index(config.get("export_profile"))
        self._profile_combobox.setCurrentIndex(profile_idx)

        self._export_btn = QtWidgets.QPushButton("Export")
        btn_hbox = QtWidgets.QHBoxLayout()
        btn_hbox.addStretch()
        btn_hbox.addWidget(self._export_btn)

        self._warning_label = QtWidgets.QLabel()
        self._warning_label.setStyleSheet("color: red")
        self._warning_label.hide()

        form = QtWidgets.QFormLayout(self)
        form.addRow("Filename:", file_box)
        form.addRow("Resolution:", self._res_combobox)
        form.addRow("Profile:", self._profile_combobox)
        form.addRow(self._warning_label)
        form.addRow(btn_hbox)

        ofile_btn.clicked.connect(self._select_ofile)
        self._export_btn.clicked.connect(self._export)
        self._ofile_text.textChanged.connect(self._check_settings)
        self._ofile_text.textChanged.connect(config.set_export_filename)
        self._res_combobox.currentIndexChanged.connect(self._set_export_res)
        self._profile_combobox.currentIndexChanged.connect(self._set_export_profile)

        self._exporter = None

    def enter(self):
        scene_info = self._get_scene_info()
        if not scene_info:
            return

        scene = scene_info.scene
        self._framerate = scene.framerate
        self._aspect_ratio = scene.aspect_ratio

        self._check_settings()

    @QtCore.Slot()
    def _check_settings(self):
        warnings = []

        if self._profile_combobox.currentData() == "gif":
            fr = self._framerate
            gif_recommended_framerate = (Fraction(25, 1), Fraction(50, 1))
            if Fraction(*fr) not in gif_recommended_framerate:
                gif_framerates = ", ".join("%s" % x for x in gif_recommended_framerate)
                warnings.append(
                    f"It is recommended to use one of these frame rate when exporting to GIF: {gif_framerates}"
                )

        if warnings:
            self._warning_label.setText("\n".join("⚠ " + w for w in warnings))
            self._warning_label.show()
        else:
            self._warning_label.hide()

    @QtCore.Slot(int)
    def _set_export_res(self, index):
        self._check_settings()
        self._config.set_export_res(self._res_combobox.currentText())

    @QtCore.Slot(int)
    def _set_export_profile(self, index):
        self._check_settings()
        self._config.set_export_profile(self._profile_combobox.currentData())

    @QtCore.Slot(int)
    def _progress(self, value):
        self._pgd.setValue(value)

    @QtCore.Slot()
    def _cancel(self):
        # Exporter.cancel() gracefuly stops the exporter thread and fires a
        # finished signal
        if self._exporter:
            self._exporter.cancel()

    @QtCore.Slot(str)
    def _fail(self, message):
        self._finish()
        QtWidgets.QMessageBox.critical(self, "Error", message, QtWidgets.QMessageBox.Ok)

    @QtCore.Slot()
    def _finish(self):
        self._pgd.close()
        if self._exporter:
            self._exporter.wait()
            self._exporter = None

    @QtCore.Slot()
    def _export(self):
        if self._exporter:
            return

        ofile = self._ofile_text.text()
        res_id = self._res_combobox.currentText()
        profile_id = self._profile_combobox.currentData()

        self._pgd = QtWidgets.QProgressDialog("Exporting to %s" % ofile, "Stop", 0, 100, self)
        self._pgd.setWindowModality(QtCore.Qt.WindowModal)
        self._pgd.setMinimumDuration(100)

        self._exporter = Exporter(self._get_scene_info, ofile, res_id, profile_id)

        self._pgd.canceled.connect(self._cancel)
        self._exporter.progressed.connect(self._progress)
        self._exporter.failed.connect(self._fail)
        self._exporter.export_finished.connect(self._finish)

        self._exporter.start()

    @QtCore.Slot()
    def _select_ofile(self):
        filenames = QtWidgets.QFileDialog.getSaveFileName(self, "Select export file")
        if not filenames[0]:
            return
        self._ofile_text.setText(filenames[0])
