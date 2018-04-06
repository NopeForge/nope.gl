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

import time

from PyQt5 import QtCore, QtWidgets


class Seekbar(QtWidgets.QWidget):

    stopped = QtCore.pyqtSignal(name='stopped')
    time_changed = QtCore.pyqtSignal(float, name='timeChanged')

    REFRESH_RATE = 60
    SLIDER_TIMEBASE = 1000
    SLIDER_TIMESCALE = 1. / SLIDER_TIMEBASE
    TIMEBASE = 1000000000  # nanoseconds
    TIMESCALE = 1. / TIMEBASE

    def _reset_clock(self):
        frame_ts = self._frame_index * self.TIMEBASE * self._framerate[1] / self._framerate[0]
        self._clock_off = int(time.time() * self.TIMEBASE) - frame_ts

    def _set_action(self, action):
        if action == 'play':
            self._reset_clock()
            self._timer.start()
            self._action_btn.setText(u'▮▮')
        elif action == 'pause':
            self._timer.stop()
            self._action_btn.setText(u'▶')
        self._current_action = action

    @QtCore.pyqtSlot(int)
    def _slider_moved(self, value):  # only user move
        if not self._scene_duration:
            return
        self._set_action('pause')
        self._frame_index = int(value * self.SLIDER_TIMESCALE * self._framerate[0] / self._framerate[1])
        self.refresh()

    @QtCore.pyqtSlot()
    def _toggle_playback(self):
        self._set_action('play' if self._current_action == 'pause' else 'pause')

    @QtCore.pyqtSlot()
    def _update_tick(self):
        now = time.time()
        now_int = int(now * self.TIMEBASE)
        media_time = now_int - self._clock_off
        if self._clock_off < 0 or media_time * self.TIMESCALE > self._scene_duration:
            self._clock_off = now_int
            media_time = 0
        self._frame_index = media_time * self._framerate[0] / (self._framerate[1] * self.TIMEBASE)
        self.refresh()

    def refresh(self):
        rendering_fps = self._framerate[0] / float(self._framerate[1])
        t = self._frame_index * 1. / rendering_fps
        cur_time = '%02d:%02d' % divmod(t, 60)
        duration = '%02d:%02d' % divmod(self._scene_duration, 60)
        self._time_lbl.setText('%s / %s (%d @ %.4gHz)' % (cur_time, duration, self._frame_index, rendering_fps))
        self._slider.setValue(t * self.SLIDER_TIMEBASE)
        self.timeChanged.emit(t)

    def _step_frame_index(self, n):
        self._set_action('pause')
        self._frame_index += n
        if self._frame_index < 0:
            self._frame_index = 0
        self.refresh()

    @QtCore.pyqtSlot()
    def _stop(self):
        self._set_action('pause')
        self.stopped.emit()

    @QtCore.pyqtSlot()
    def _step_fw(self):
        self._step_frame_index(1)

    @QtCore.pyqtSlot()
    def _step_bw(self):
        self._step_frame_index(-1)

    def pause(self):
        self._set_action('pause')

    def set_duration(self, duration):
        self._scene_duration = duration
        self._slider.setRange(0, duration * self.SLIDER_TIMEBASE)
        self.refresh()

    def set_framerate(self, framerate):
        self._framerate = framerate
        assert framerate[1]
        self.refresh()

    def set_frame_index(self, frame_index):
        self._frame_index = frame_index
        self.refresh()

    def get_frame_index(self):
        return self._frame_index

    def __init__(self, framerate, stop_button=True):
        super(Seekbar, self).__init__()

        self._timer = QtCore.QTimer()
        self._timer.setInterval(1000.0 / self.REFRESH_RATE)  # in milliseconds

        self._slider = QtWidgets.QSlider(QtCore.Qt.Horizontal)
        self._time_lbl = QtWidgets.QLabel()

        stop_btn = QtWidgets.QToolButton()
        stop_btn.setText(u'◾')
        self._action_btn = QtWidgets.QToolButton()
        self._set_action('pause')

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
        self._clock_off = -1
        self._scene_duration = 0
        self.set_framerate(framerate)

        self._slider.sliderMoved.connect(self._slider_moved)

        stop_btn.clicked.connect(self._stop)
        self._action_btn.clicked.connect(self._toggle_playback)
        fw_btn.clicked.connect(self._step_fw)
        bw_btn.clicked.connect(self._step_bw)

        self._timer.timeout.connect(self._update_tick)
