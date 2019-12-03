#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2018 GoPro Inc.
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

from PySide2 import QtCore, QtWidgets


class Seekbar(QtWidgets.QWidget):

    play = QtCore.Signal()
    pause = QtCore.Signal()
    seek = QtCore.Signal(float)
    step = QtCore.Signal(int)
    stop = QtCore.Signal()

    SLIDER_TIMEBASE = 1000
    SLIDER_TIMESCALE = 1. / SLIDER_TIMEBASE

    def __init__(self, config, stop_button=True):
        super(Seekbar, self).__init__()

        self._slider = QtWidgets.QSlider(QtCore.Qt.Horizontal)
        self._time_lbl = QtWidgets.QLabel()

        stop_btn = QtWidgets.QToolButton()
        stop_btn.setText(u'■')
        self._action_btn = QtWidgets.QToolButton()

        fw_btn = QtWidgets.QToolButton()
        fw_btn.setText('>')
        bw_btn = QtWidgets.QToolButton()
        bw_btn.setText('<')

        layout = QtWidgets.QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        if stop_button:
            layout.addWidget(stop_btn)
        layout.addWidget(bw_btn)
        layout.addWidget(self._action_btn)
        layout.addWidget(fw_btn)
        layout.addWidget(self._slider)
        layout.addWidget(self._time_lbl)

        self._frame_index = 0
        self._scene_duration = 0
        self._framerate = config.get('framerate')
        self._set_action('pause')

        self._slider.sliderMoved.connect(self._slider_moved)
        self._slider.sliderPressed.connect(self._slider_pressed)
        self._slider.sliderReleased.connect(self._slider_released)
        self._slider_dragged = False

        stop_btn.clicked.connect(self._stop)
        self._action_btn.clicked.connect(self._toggle_playback)
        fw_btn.clicked.connect(self._step_fw)
        bw_btn.clicked.connect(self._step_bw)

    def _set_action(self, action):
        if action == 'play':
            self._action_btn.setText(u'▮▮')
        elif action == 'pause':
            self._action_btn.setText(u'▶')
        self._current_state = action

    @QtCore.Slot(int)
    def _slider_moved(self, value):  # only user move
        if not self._scene_duration:
            return
        self.seek.emit(value * self.SLIDER_TIMESCALE)

    @QtCore.Slot()
    def _slider_pressed(self):
        self._slider_dragged = True

    @QtCore.Slot()
    def _slider_released(self):
        self._slider_dragged = False
        self._refresh()

    @QtCore.Slot()
    def _toggle_playback(self):
        if self._current_state == 'play':
            self.pause.emit()
        else:
            self.play.emit()

    @QtCore.Slot()
    def _stop(self):
        self._set_action('pause')
        self.stop.emit()

    @QtCore.Slot()
    def _step_fw(self):
        self.step.emit(1)

    @QtCore.Slot()
    def _step_bw(self):
        self.step.emit(-1)

    @QtCore.Slot(dict)
    def set_scene_metadata(self, cfg):
        self._scene_duration = cfg['duration']
        self._framerate = cfg['framerate']
        self._slider.setRange(0, self._scene_duration * self.SLIDER_TIMEBASE)
        self._refresh()

    @QtCore.Slot(int, float)
    def set_frame_time(self, frame_index, frame_time):
        self._frame_index = frame_index
        self._refresh()

    @QtCore.Slot()
    def set_play_state(self):
        self._set_action('play')

    @QtCore.Slot()
    def set_pause_state(self):
        self._set_action('pause')

    def _refresh(self):
        if not self._framerate:
            return
        rendering_fps = self._framerate[0] / float(self._framerate[1])
        t = self._frame_index * 1. / rendering_fps
        cur_time = '%02d:%02d' % divmod(t, 60)
        duration = '%02d:%02d' % divmod(self._scene_duration, 60)
        self._time_lbl.setText('%s / %s (%d @ %.4gHz)' % (cur_time, duration, self._frame_index, rendering_fps))
        if not self._slider_dragged:
            self._slider.setValue(t * self.SLIDER_TIMEBASE)
