#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2019 GoPro Inc.
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
from PyQt5 import QtCore, QtGui, QtWidgets


class _ControlWidget(QtWidgets.QWidget):

    need_scene_reload = QtCore.pyqtSignal(str, object, name='needSceneReload')

    def __init__(self, name):
        super(_ControlWidget, self).__init__()
        self._name = name
        self.layout = QtWidgets.QVBoxLayout(self)
        self.layout.setContentsMargins(0, 0, 0, 0)

    def signal_change(self, value):
        self.need_scene_reload.emit(self._name, value)

    def get_label_text(self, value=None):
        return '<b>{}:</b> {}'.format(self._name, value) if value is not None else '<b>{}:</b>'.format(self._name)


class Slider(_ControlWidget):

    def __init__(self, name, value, **kwargs):
        super(Slider, self).__init__(name)
        slider = QtWidgets.QSlider(QtCore.Qt.Horizontal)
        self._unit_base = kwargs.get('unit_base', 1)
        if 'range' in kwargs:
            srange = kwargs['range']
            slider.setRange(srange[0] * self._unit_base, srange[1] * self._unit_base)
        slider.setValue(value * self._unit_base)
        self._label = QtWidgets.QLabel(self.get_label_text(value))
        slider.valueChanged.connect(self._slider_value_changed)
        self.layout.addWidget(self._label)
        self.layout.addWidget(slider)

    @QtCore.pyqtSlot(int)
    def _slider_value_changed(self, value):
        real_value = value if self._unit_base is 1 else value / float(self._unit_base)
        self._label.setText(self.get_label_text(real_value))
        self.signal_change(real_value)


class ColorPicker(_ControlWidget):

    def __init__(self, name, value, **kwargs):
        super(ColorPicker, self).__init__(name)
        self._color_btn = QtWidgets.QPushButton()
        color = QtGui.QColor()
        color.setRgbF(*value)
        color_name = color.name()
        self._color_btn.setStyleSheet('background-color: %s;' % color_name)
        self._label = QtWidgets.QLabel(self.get_label_text(color_name))
        self._color_btn.pressed.connect(self._pick_color)
        self._qcolor = color
        self.layout.addWidget(self._label)
        self.layout.addWidget(self._color_btn)

    @QtCore.pyqtSlot()
    def _pick_color(self):
        color = QtWidgets.QColorDialog.getColor(initial=self._qcolor)
        if not color.isValid():
            return
        self._color_btn.setStyleSheet('background-color: %s;' % color.name())
        self._qcolor = color
        self._label.setText(self.get_label_text(color.name()))
        self.signal_change(color.getRgbF())


class Checkbox(_ControlWidget):

    def __init__(self, name, value, **kwargs):
        super(Checkbox, self).__init__(name)
        self._chkbox = QtWidgets.QCheckBox(name)
        self._chkbox.setChecked(value)
        self._chkbox.stateChanged.connect(self._checkbox_toggle)
        self.layout.addWidget(self._chkbox)

    @QtCore.pyqtSlot()
    def _checkbox_toggle(self):
        self.signal_change(self._chkbox.isChecked())


class FilePicker(_ControlWidget):

    def __init__(self, name, value, **kwargs):
        super(FilePicker, self).__init__(name)
        self._filter = kwargs.get('filter', '')
        dialog_btn = QtWidgets.QPushButton('Open file')
        self._label = QtWidgets.QLabel(self.get_label_text(value))
        dialog_btn.pressed.connect(self._choose_filename)
        self.layout.addWidget(self._label)
        self.layout.addWidget(dialog_btn)

    @QtCore.pyqtSlot()
    def _choose_filename(self):
        filenames = QtWidgets.QFileDialog.getOpenFileName(self, 'Open file', '', self._filter)
        if not filenames[0]:
            return
        self._label.setText(self.get_label_text(op.basename(filenames[0])))
        self.signal_change(filenames[0])


class ComboBox(_ControlWidget):

    def __init__(self, name, value, **kwargs):
        super(ComboBox, self).__init__(name)
        combobox = QtWidgets.QComboBox()
        label = QtWidgets.QLabel(self.get_label_text())
        choices = kwargs['choices']
        combobox.addItems(choices)
        combobox.setCurrentIndex(choices.index(value))
        combobox.currentTextChanged.connect(self._combobox_select)
        self.layout.addWidget(label)
        self.layout.addWidget(combobox)

    @QtCore.pyqtSlot(str)
    def _combobox_select(self, text):
        self.signal_change(text)


class TextInput(_ControlWidget):

    def __init__(self, name, value, **kwargs):
        super(TextInput, self).__init__(name)
        self._text = QtWidgets.QPlainTextEdit()
        self._text.setPlainText(value)
        label = QtWidgets.QLabel(self.get_label_text())
        self.layout.addWidget(label)
        self.layout.addWidget(self._text)
        submit_btn = QtWidgets.QPushButton('OK')
        submit_btn.pressed.connect(self._submit_text)
        self.layout.addWidget(label)
        self.layout.addWidget(self._text)
        self.layout.addWidget(submit_btn)

    @QtCore.pyqtSlot()
    def _submit_text(self):
        self.signal_change(self._text.toPlainText())
