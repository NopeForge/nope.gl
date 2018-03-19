#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2016 GoPro Inc.
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
import platform
import sys
import time
import subprocess
from fractions import Fraction

import pynodegl as ngl

from misc import Media
from export import Exporter
from gl import get_gl_format
from scriptsmgr import ScriptsManager
from config import Config
from com import query_subproc

from PyQt5 import QtGui, QtCore, QtWidgets, QtSvg
from OpenGL import GL


class _SerialView(QtWidgets.QWidget):

    @QtCore.pyqtSlot()
    def _save_to_file(self):
        data = self._text.toPlainText()
        filenames = QtWidgets.QFileDialog.getSaveFileName(self, 'Select export file')
        if not filenames[0]:
            return
        open(filenames[0], 'w').write(data)

    def refresh(self):
        cfg = self._get_scene_func()
        if not cfg:
            return
        self._text.setPlainText(cfg['scene'])

    def __init__(self, get_scene_func):
        super(_SerialView, self).__init__()

        self._get_scene_func = get_scene_func

        self._save_btn = QtWidgets.QPushButton("Save to file")
        self._graph_lbl = QtWidgets.QLabel()
        self._text = QtWidgets.QPlainTextEdit()
        self._text.setFont(QtGui.QFontDatabase.systemFont(QtGui.QFontDatabase.FixedFont))
        self._text.setReadOnly(True)

        hbox = QtWidgets.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget(self._save_btn)

        serial_layout = QtWidgets.QVBoxLayout(self)
        serial_layout.addWidget(self._text)
        serial_layout.addLayout(hbox)

        self._save_btn.clicked.connect(self._save_to_file)


class _GLWidget(QtWidgets.QOpenGLWidget):

    def get_time(self):
        return self._time

    def set_time(self, t):
        self._time = t
        self.update()

    def reset_viewer(self):
        self.makeCurrent()
        del self._viewer
        self._viewer = ngl.Viewer()
        self.initializeGL()
        self.doneCurrent()
        self.update()

    def set_scene(self, scene):
        self.makeCurrent()
        self._viewer.set_scene_from_string(scene)
        self.doneCurrent()
        self.update()

    def __init__(self, parent, aspect_ratio, samples, clear_color):
        super(_GLWidget, self).__init__(parent)

        self.setMinimumSize(640, 360)
        self._viewer = ngl.Viewer()
        self._time = 0
        self._aspect_ratio = aspect_ratio
        self._samples = samples
        self._clear_color = clear_color
        gl_format = QtGui.QSurfaceFormat.defaultFormat()
        gl_format.setSamples(samples)
        self.setFormat(gl_format)
        self.resizeGL(self.width(), self.height())

    def paintGL(self):
        GL.glViewport(self.view_x, self.view_y, self.view_width, self.view_height)
        self._viewer.draw(self._time)

    def resizeGL(self, screen_width, screen_height):
        screen_width = int(screen_width * self.devicePixelRatioF())
        screen_height = int(screen_height * self.devicePixelRatioF())
        aspect = self._aspect_ratio
        self.view_width = screen_width
        self.view_height = screen_width * aspect[1] / aspect[0]

        if self.view_height > screen_height:
            self.view_height = screen_height
            self.view_width = screen_height * aspect[0] / aspect[1]

        self.view_x = (screen_width - self.view_width) // 2
        self.view_y = (screen_height - self.view_height) // 2

    def initializeGL(self):
        api = ngl.GLAPI_OPENGL3
        if self.context().isOpenGLES():
            api = ngl.GLAPI_OPENGLES2
        GL.glClearColor(*self._clear_color)
        self._viewer.configure(ngl.GLPLATFORM_AUTO, api)


class _ExportView(QtWidgets.QWidget):

    def _get_encoders_list(self):
        encoders = ['']
        stdout = subprocess.check_output(['ffmpeg', '-v', '0', '-encoders'])
        list_started = False
        for line in stdout.splitlines():
            line = line.lstrip()
            if not list_started:
                if line.startswith('------'):
                    list_started = True
                continue
            caps, encoder, desc = line.split(None, 2)
            if caps[0] != 'V':
                continue
            encoders.append(encoder)
        return encoders

    @QtCore.pyqtSlot()
    def _export(self):
        ofile  = self._ofile_text.text()
        width  = self._spinbox_width.value()
        height = self._spinbox_height.value()

        extra_enc_args = []
        encoder_id = self._encoders_cbox.currentIndex()
        if encoder_id:
            extra_enc_args += ['-c:v', self._encoders_cbox.itemText(encoder_id)]
        extra_enc_args += self._encopts_text.text().split()

        self._pgbar.setValue(0)
        self._pgbar.show()
        cfg = self._exporter.export(ofile, width, height, extra_enc_args)
        if not cfg:
            QtWidgets.QMessageBox.critical(self, 'Error',
                                           "You didn't select any scene to export.",
                                           QtWidgets.QMessageBox.Ok)
        self._pgbar.hide()

    @QtCore.pyqtSlot()
    def _select_ofile(self):
        filenames = QtWidgets.QFileDialog.getSaveFileName(self, 'Select export file')
        if not filenames[0]:
            return
        self._ofile_text.setText(filenames[0])

    def __init__(self, parent, get_scene_func):
        super(_ExportView, self).__init__()

        self.parent = parent

        self._ofile_text = QtWidgets.QLineEdit('/tmp/ngl-export.mp4')
        ofile_btn = QtWidgets.QPushButton('Browse')

        file_box = QtWidgets.QHBoxLayout()
        file_box.addWidget(self._ofile_text)
        file_box.addWidget(ofile_btn)

        self._spinbox_width = QtWidgets.QSpinBox()
        self._spinbox_width.setRange(1, 0xffff)
        self._spinbox_width.setValue(800)

        self._spinbox_height = QtWidgets.QSpinBox()
        self._spinbox_height.setRange(1, 0xffff)
        self._spinbox_height.setValue(600)

        self._encoders_cbox = QtWidgets.QComboBox()
        self._encoders_cbox.addItems(self._get_encoders_list())

        self._encopts_text = QtWidgets.QLineEdit()

        self._export_btn = QtWidgets.QPushButton('Export')

        self._pgbar = QtWidgets.QProgressBar()
        self._pgbar.hide()

        form = QtWidgets.QFormLayout(self)
        form.addRow('Filename:', file_box)
        form.addRow('Width:',    self._spinbox_width)
        form.addRow('Height:',   self._spinbox_height)
        form.addRow('Encoder:',  self._encoders_cbox)
        form.addRow('Extra encoder arguments:', self._encopts_text)
        form.addRow(self._export_btn)
        form.addRow(self._pgbar)

        self._exporter = Exporter(get_scene_func)
        self._exporter.progressed.connect(self._pgbar.setValue)

        ofile_btn.clicked.connect(self._select_ofile)
        self._export_btn.clicked.connect(self._export)


class _SVGGraphView(QtWidgets.QGraphicsView):

    def wheelEvent(self, event):
        factor = 1.25
        if event.angleDelta().y() < 0:
            factor = 1.0 / factor
        self.scale(factor, factor)

    def __init__(self):
        super(_SVGGraphView, self).__init__()
        self.setDragMode(QtWidgets.QGraphicsView.ScrollHandDrag)


class _GraphView(QtWidgets.QWidget):

    @QtCore.pyqtSlot()
    def _save_to_file(self):
        filenames = QtWidgets.QFileDialog.getSaveFileName(self, 'Select export file')
        if not filenames[0]:
            return
        rect_size = self._scene.sceneRect().size()
        img_size = QtCore.QSize(int(rect_size.width()), int(rect_size.height()))
        img = QtGui.QImage(img_size, QtGui.QImage.Format_ARGB32_Premultiplied)
        painter = QtGui.QPainter(img)
        self._scene.render(painter)
        img.save(filenames[0])
        painter.end()

    def refresh(self):
        cfg = self._get_scene_func(fmt='dot')
        if not cfg:
            return

        basename = '/tmp/ngl_scene.'
        dotfile = basename + 'dot'
        svgfile = basename + 'svg'
        open(dotfile, 'w').write(cfg['scene'])
        try:
            subprocess.call(['dot', '-Tsvg', dotfile, '-o' + svgfile])
        except OSError, e:
            QtWidgets.QMessageBox.critical(self, 'Graphviz error',
                                          'Error while executing dot (Graphviz): %s' % e.strerror,
                                           QtWidgets.QMessageBox.Ok)
            return

        item = QtSvg.QGraphicsSvgItem(svgfile)
        self._scene.clear()
        self._scene.addItem(item)
        self._scene.setSceneRect(item.boundingRect())

    def __init__(self, get_scene_func):
        super(_GraphView, self).__init__()

        self._get_scene_func = get_scene_func

        self._save_btn = QtWidgets.QPushButton("Save image")

        self._scene = QtWidgets.QGraphicsScene()
        self._view = _SVGGraphView()
        self._view.setScene(self._scene)

        hbox = QtWidgets.QHBoxLayout()
        hbox.addStretch()
        hbox.addWidget(self._save_btn)

        graph_layout = QtWidgets.QVBoxLayout(self)
        graph_layout.addWidget(self._view)
        graph_layout.addLayout(hbox)

        self._save_btn.clicked.connect(self._save_to_file)


class _GLView(QtWidgets.QWidget):

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
            self._action_btn.setText(u'❙❙')
        elif action == 'pause':
            self._timer.stop()
            self._action_btn.setText(u'►')
        self._current_action = action

    @QtCore.pyqtSlot(int)
    def _slider_moved(self, value):  # only user move
        if not self._scene_duration:
            return
        self._set_action('pause')
        self._frame_index = int(value * self.SLIDER_TIMESCALE * self._framerate[0] / self._framerate[1])
        self._reset_clock()
        self._refresh()

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
        self._refresh()

    @QtCore.pyqtSlot()
    def _refresh(self):
        rendering_fps = self._framerate[0] / float(self._framerate[1])
        t = self._frame_index * 1. / rendering_fps
        if self._gl_widget:
            self._gl_widget.set_time(t)
        cur_time = '%02d:%02d' % divmod(t, 60)
        duration = '%02d:%02d' % divmod(self._scene_duration, 60)
        self._time_lbl.setText('%s / %s (%d @ %.4gHz)' % (cur_time, duration, self._frame_index, rendering_fps))
        self._slider.setValue(t * self.SLIDER_TIMEBASE)

    def _step_frame_index(self, n):
        self._set_action('pause')
        self._frame_index += n
        if self._frame_index < 0:
            self._frame_index = 0
        self._refresh()

    @QtCore.pyqtSlot()
    def _stop(self):
        self._set_action('pause')
        self._gl_widget.reset_viewer()

    @QtCore.pyqtSlot()
    def _step_fw(self):
        self._step_frame_index(1)

    @QtCore.pyqtSlot()
    def _step_bw(self):
        self._step_frame_index(-1)

    @QtCore.pyqtSlot()
    def _screenshot(self):
        filenames = QtWidgets.QFileDialog.getSaveFileName(self, 'Save screenshot file')
        if not filenames[0]:
            return
        self._gl_widget.grabFramebuffer().save(filenames[0])

    def _recreate_gl_widget(self):
        gl_widget = _GLWidget(self, self._ar, self._samples, self._clear_color)
        gl_widget.set_time(self._gl_widget.get_time())
        self._gl_layout.replaceWidget(self._gl_widget, gl_widget)
        self._gl_widget.setParent(None)
        self._gl_widget = gl_widget

    @QtCore.pyqtSlot(tuple)
    def set_aspect_ratio(self, ar):
        self._ar = ar
        self._recreate_gl_widget()

    @QtCore.pyqtSlot(tuple)
    def set_frame_rate(self, fr):
        self._framerate = fr

    @QtCore.pyqtSlot(int)
    def set_samples(self, samples):
        self._samples = samples
        self._recreate_gl_widget()

    @QtCore.pyqtSlot(tuple)
    def set_clear_color(self, color):
        self._clear_color = color
        self._recreate_gl_widget()

    def refresh(self):
        cfg = self._get_scene_func()
        if not cfg:
            return
        if Fraction(*cfg['aspect_ratio']) != Fraction(*self._ar):
            self.set_aspect_ratio(cfg['aspect_ratio'])
        self._gl_widget.set_scene(cfg['scene'])
        self._scene_duration = cfg['duration']
        self._slider.setRange(0, self._scene_duration * self.SLIDER_TIMEBASE)
        self._refresh()

    def __init__(self, get_scene_func, config):
        super(_GLView, self).__init__()

        self._ar = config.get('aspect_ratio')
        self._framerate = config.get('framerate')
        self._samples = config.get('samples')
        self._clear_color = config.get('clear_color')

        self._get_scene_func = get_scene_func

        self._frame_index = 0
        self._clock_off = -1
        self._scene_duration = 0

        self._timer = QtCore.QTimer()
        self._timer.setInterval(1000.0 / self.REFRESH_RATE)  # in milliseconds

        self._gl_widget = _GLWidget(self, self._ar, self._samples, self._clear_color)

        self._slider = QtWidgets.QSlider(QtCore.Qt.Horizontal)

        stop_btn = QtWidgets.QPushButton('◾')
        self._action_btn = QtWidgets.QPushButton()
        self._set_action('pause')

        fw_btn = QtWidgets.QPushButton('>')
        bw_btn = QtWidgets.QPushButton('<')

        self._time_lbl = QtWidgets.QLabel()

        screenshot_btn = QtWidgets.QPushButton('Screenshot')

        toolbar = QtWidgets.QHBoxLayout()
        toolbar.addWidget(stop_btn)
        toolbar.addWidget(bw_btn)
        toolbar.addWidget(self._action_btn)
        toolbar.addWidget(fw_btn)
        toolbar.addWidget(self._time_lbl)
        toolbar.addStretch()
        toolbar.addWidget(screenshot_btn)

        self._gl_layout = QtWidgets.QVBoxLayout(self)
        self._gl_layout.addWidget(self._gl_widget, stretch=1)
        self._gl_layout.addWidget(self._slider)
        self._gl_layout.addLayout(toolbar)

        self._refresh()

        self._timer.timeout.connect(self._update_tick)

        stop_btn.clicked.connect(self._stop)
        self._action_btn.clicked.connect(self._toggle_playback)
        fw_btn.clicked.connect(self._step_fw)
        bw_btn.clicked.connect(self._step_bw)
        self._slider.sliderMoved.connect(self._slider_moved)
        screenshot_btn.clicked.connect(self._screenshot)


class _Toolbar(QtWidgets.QWidget):

    scene_changed = QtCore.pyqtSignal(str, str, name='sceneChanged')
    aspect_ratio_changed = QtCore.pyqtSignal(tuple, name='aspectRatioChanged')
    samples_changed = QtCore.pyqtSignal(int, name='samplesChanged')
    frame_rate_changed = QtCore.pyqtSignal(tuple, name='frameRateChanged')
    log_level_changed = QtCore.pyqtSignal(str, name='logLevelChanged')
    clear_color_changed = QtCore.pyqtSignal(tuple, name='clearColorChanged')

    def _replace_scene_opts_widget(self, widget):
        if self._scene_opts_widget:
            self._scene_toolbar_layout.removeWidget(self._scene_opts_widget)
            self._scene_opts_widget.deleteLater()
            self._scene_opts_widget = None
        if widget:
            self._scene_toolbar_layout.addWidget(widget)
            self._scene_opts_widget = widget
            widget.show()

    def _get_label_text(self, id_name, value):
        return '<b>{}:</b> {}'.format(id_name, value)

    def _craft_slider_value_changed_cb(self, id_name, label, unit_base):

        @QtCore.pyqtSlot(int)
        def slider_value_changed(value):
            real_value = value if unit_base is 1 else value / float(unit_base)
            self._scene_extra_args[id_name] = real_value
            label.setText(self._get_label_text(id_name, real_value))
            self._load_current_scene(load_widgets=False)

        return slider_value_changed

    def _craft_pick_color_cb(self, id_name, label, color_btn):

        @QtCore.pyqtSlot()
        def pick_color():
            color = QtWidgets.QColorDialog.getColor()
            color_btn.setStyleSheet('background-color: %s;' % color.name())
            label.setText(self._get_label_text(id_name, color.name()))
            self._scene_extra_args[id_name] = color.getRgbF()
            self._load_current_scene(load_widgets=False)

        return pick_color

    def _craft_choose_filename_cb(self, id_name, label, dialog_btn, files_filter):

        @QtCore.pyqtSlot()
        def choose_filename():
            filenames = QtWidgets.QFileDialog.getOpenFileName(self, 'Open file', '', files_filter)
            if not filenames[0]:
                return
            label.setText(self._get_label_text(id_name, op.basename(filenames[0])))
            self._scene_extra_args[id_name] = filenames[0]
            self._load_current_scene(load_widgets=False)

        return choose_filename

    def _craft_checkbox_toggle_cb(self, id_name, chkbox):

        @QtCore.pyqtSlot()
        def checkbox_toggle():
            self._scene_extra_args[id_name] = chkbox.isChecked()
            self._load_current_scene(load_widgets=False)

        return checkbox_toggle

    def _get_opts_widget_from_specs(self, widgets_specs):
        widgets = []

        for widget_specs in widgets_specs:
            name = widget_specs['name']
            default_value = widget_specs['default']

            if widget_specs['type'] == 'range':
                slider = QtWidgets.QSlider(QtCore.Qt.Horizontal)
                unit_base = widget_specs.get('unit_base', 1)
                if 'range' in widget_specs:
                    srange = widget_specs['range']
                    slider.setRange(srange[0] * unit_base, srange[1] * unit_base)
                slider.setValue(default_value * unit_base)
                label = QtWidgets.QLabel(self._get_label_text(name, default_value))
                widgets.append(label)
                slider_value_changed_cb = self._craft_slider_value_changed_cb(name, label, unit_base)
                slider.valueChanged.connect(slider_value_changed_cb)
                widgets.append(slider)

            elif widget_specs['type'] == 'color':
                color_btn = QtWidgets.QPushButton()
                color = QtGui.QColor()
                color.setRgbF(*default_value)
                color_name = color.name()
                color_btn.setStyleSheet('background-color: %s;' % color_name)
                label = QtWidgets.QLabel(self._get_label_text(name, color_name))
                widgets.append(label)
                pick_color_cb = self._craft_pick_color_cb(name, label, color_btn)
                color_btn.pressed.connect(pick_color_cb)
                widgets.append(color_btn)

            elif widget_specs['type'] == 'bool':
                chkbox = QtWidgets.QCheckBox(name)
                chkbox.setChecked(default_value)
                checkbox_toggle_cb = self._craft_checkbox_toggle_cb(name, chkbox)
                chkbox.stateChanged.connect(checkbox_toggle_cb)
                widgets.append(chkbox)

            elif widget_specs['type'] == 'file':
                dialog_btn = QtWidgets.QPushButton('Open file')
                label = QtWidgets.QLabel(self._get_label_text(name, default_value))
                widgets.append(label)
                choose_filename_cb = self._craft_choose_filename_cb(name, label, dialog_btn, widget_specs.get('filter', ''))
                dialog_btn.pressed.connect(choose_filename_cb)
                widgets.append(dialog_btn)

        if not widgets:
            return None

        groupbox = QtWidgets.QGroupBox('Custom scene options')
        vbox = QtWidgets.QVBoxLayout()
        for widget in widgets:
            vbox.addWidget(widget)
        groupbox.setLayout(vbox)
        return groupbox

    def get_scene_cfg(self):
        choices = Config.CHOICES
        return {
                'scene': self._current_scene_data,
                'aspect_ratio': choices['aspect_ratio'][self._ar_cbbox.currentIndex()],
                'framerate': choices['framerate'][self._fr_cbbox.currentIndex()],
                'samples': choices['samples'][self._samples_cbbox.currentIndex()],
                'extra_args': self._scene_extra_args,
                'has_fps': self._fps_chkbox.isChecked(),
        }

    def load_scene_from_name(self, module_name, scene_name):

        def get_func_info(module_item, func_name):
            nb_funcs = module_item.rowCount()
            for i in range(nb_funcs):
                func_item = module_item.child(i)
                if func_item.text() == scene_name:
                    return func_item.data()
            return None

        module_items = self._scn_mdl.findItems(module_name)
        if module_items:
            self._current_scene_data = get_func_info(module_items[0], scene_name)
            self._load_current_scene()

    def _load_current_scene(self, load_widgets=True):
        if not self._current_scene_data:
            return
        module_name, scene_name, widgets_specs = self._current_scene_data
        if load_widgets:
            self._scene_extra_args = {}
            scene_opts_widget = self._get_opts_widget_from_specs(widgets_specs)
            self._replace_scene_opts_widget(scene_opts_widget)
        self.sceneChanged.emit(module_name, scene_name)

    def clear_scripts(self):
        self._scn_mdl.clear()

    def _reload_scene_view(self, scenes):
        self._scn_mdl.clear()
        for module_name, sub_scenes in scenes:
            qitem_script = QtGui.QStandardItem(module_name)
            for scene_name, widgets_specs in sub_scenes:
                scene_data = (module_name, scene_name, widgets_specs)

                # update cached widget specs if module and scene match
                if self._current_scene_data:
                    cur_module_name, cur_scene_name, _ = self._current_scene_data
                    if module_name == cur_module_name and scene_name == cur_scene_name:
                        self._current_scene_data = scene_data

                qitem_func = QtGui.QStandardItem(scene_name)
                qitem_script.appendRow(qitem_func)
                qitem_func.setData(scene_data)
            self._scn_mdl.appendRow(qitem_script)
        self._scn_view.expandAll()

    @QtCore.pyqtSlot(QtCore.QModelIndex)
    def _scn_view_selected(self, index):
        data = self._scn_mdl.itemFromIndex(index).data()
        if data:
            self._current_scene_data = data
            self._load_current_scene()

    @QtCore.pyqtSlot(list)
    def on_scripts_changed(self, scenes):
        self._reload_scene_view(scenes)
        self._load_current_scene()

    def set_cfg(self, cfg):
        try:
            cfg_ar = Fraction(*cfg['aspect_ratio'])
            cfg_ar = (cfg_ar.numerator, cfg_ar.denominator)
            ar = Fraction(*Config.CHOICES['aspect_ratio'][self._ar_cbbox.currentIndex()])
            ar = (ar.numerator, ar.denominator)
            if ar != cfg_ar:
                self._far_lbl2.setText('%d:%d' % cfg_ar)
                self._far_lbl.setVisible(True)
                self._far_lbl2.setVisible(True)
            else:
                self._far_lbl.setVisible(False)
                self._far_lbl2.setVisible(False)
        except ValueError:
            pass

    @QtCore.pyqtSlot()
    def _fps_chkbox_changed(self):
        self._load_current_scene()

    @QtCore.pyqtSlot(int)
    def _set_loglevel(self, index):
        level_str = Config.CHOICES['log_level'][index]
        ngl_level = eval('ngl.LOG_%s' % level_str.upper())
        ngl.log_set_min_level(ngl_level)
        self.logLevelChanged.emit(level_str)

    @QtCore.pyqtSlot(int)
    def _set_aspect_ratio(self, index):
        ar = Config.CHOICES['aspect_ratio'][index]
        self.aspectRatioChanged.emit(ar)
        self._load_current_scene()

    @QtCore.pyqtSlot(int)
    def _set_frame_rate(self, index):
        fr = Config.CHOICES['framerate'][index]
        self.frameRateChanged.emit(fr)
        self._load_current_scene()

    @QtCore.pyqtSlot(int)
    def _set_samples(self, index):
        samples = Config.CHOICES['samples'][index]
        self.samplesChanged.emit(samples)
        self._load_current_scene()

    @QtCore.pyqtSlot()
    def _set_clear_color(self):
        color = QtWidgets.QColorDialog.getColor()
        color_name = color.name()
        self._clearcolor_btn.setStyleSheet('background-color: %s;' % color_name)
        self.clearColorChanged.emit(color.getRgbF())
        self._load_current_scene()

    def __init__(self, config):
        super(_Toolbar, self).__init__()

        self._scene_opts_widget = None
        self._scene_extra_args = {}

        self._scn_view = QtWidgets.QTreeView()
        self._scn_view.setHeaderHidden(True)
        self._scn_view.setSelectionBehavior(QtWidgets.QAbstractItemView.SelectRows)
        self._scn_view.setEditTriggers(QtWidgets.QAbstractItemView.NoEditTriggers)
        self._scn_mdl = QtGui.QStandardItemModel()
        self._scn_view.setModel(self._scn_mdl)

        self._current_scene_data = None

        self._fps_chkbox = QtWidgets.QCheckBox('Show FPS')

        all_ar = config.CHOICES['aspect_ratio']
        default_ar = config.get('aspect_ratio')
        self._ar_cbbox = QtWidgets.QComboBox()
        for ar in all_ar:
            self._ar_cbbox.addItem('%d:%d' % ar)
        self._ar_cbbox.setCurrentIndex(all_ar.index(default_ar))
        ar_lbl = QtWidgets.QLabel('Aspect ratio:')
        ar_hbox = QtWidgets.QHBoxLayout()
        ar_hbox.addWidget(ar_lbl)
        ar_hbox.addWidget(self._ar_cbbox)

        self._far_lbl = QtWidgets.QLabel('Forced aspect ratio:')
        self._far_lbl.setStyleSheet("color: red;")
        self._far_lbl.setVisible(False)
        self._far_lbl2 = QtWidgets.QLabel('1:1')
        self._far_lbl2.setStyleSheet("color: red;")
        self._far_lbl2.setVisible(False)
        far_hbox = QtWidgets.QHBoxLayout()
        far_hbox.addWidget(self._far_lbl)
        far_hbox.addWidget(self._far_lbl2)

        all_samples = config.CHOICES['samples']
        default_samples = config.get('samples')
        self._samples_cbbox = QtWidgets.QComboBox()
        for samples in all_samples:
            self._samples_cbbox.addItem('%dx' % samples if samples else 'Disabled')
        self._samples_cbbox.setCurrentIndex(all_samples.index(default_samples))
        samples_lbl = QtWidgets.QLabel('MSAA:')
        samples_hbox = QtWidgets.QHBoxLayout()
        samples_hbox.addWidget(samples_lbl)
        samples_hbox.addWidget(self._samples_cbbox)

        all_fr = config.CHOICES['framerate']
        default_fr = config.get('framerate')
        self._fr_cbbox = QtWidgets.QComboBox()
        for fr in all_fr:
            self._fr_cbbox.addItem('%.5g FPS' % (fr[0] / float(fr[1])))
        self._fr_cbbox.setCurrentIndex(all_fr.index(default_fr))
        fr_lbl = QtWidgets.QLabel('Frame rate:')
        fr_hbox = QtWidgets.QHBoxLayout()
        fr_hbox.addWidget(fr_lbl)
        fr_hbox.addWidget(self._fr_cbbox)

        all_loglevels = config.CHOICES['log_level']
        default_loglevel = config.get('log_level')
        self._loglevel_cbbox = QtWidgets.QComboBox()
        for level in all_loglevels:
            self._loglevel_cbbox.addItem(level.title())
        log_level_idx = all_loglevels.index(default_loglevel)
        self._loglevel_cbbox.setCurrentIndex(log_level_idx)
        self._set_loglevel(log_level_idx)
        loglevel_lbl = QtWidgets.QLabel('Min log level:')
        loglevel_hbox = QtWidgets.QHBoxLayout()
        loglevel_hbox.addWidget(loglevel_lbl)
        loglevel_hbox.addWidget(self._loglevel_cbbox)

        # TODO: factor out widget (see _get_opts_widget_from_specs)
        default_clearcolor = config.get('clear_color')
        self._clearcolor_btn = QtWidgets.QPushButton()
        color = QtGui.QColor()
        color.setRgbF(*default_clearcolor)
        color_name = color.name()
        self._clearcolor_btn.setStyleSheet('background-color: %s;' % color_name)
        self._clearcolor_btn.pressed.connect(self._set_clear_color)

        clearcolor_lbl = QtWidgets.QLabel('Clear color:')
        clearcolor_hbox = QtWidgets.QHBoxLayout()
        clearcolor_hbox.addWidget(clearcolor_lbl)
        clearcolor_hbox.addWidget(self._clearcolor_btn)

        self.reload_btn = QtWidgets.QPushButton('Force scripts reload')

        self._scene_toolbar_layout = QtWidgets.QVBoxLayout(self)
        self._scene_toolbar_layout.addWidget(self._fps_chkbox)
        self._scene_toolbar_layout.addLayout(ar_hbox)
        self._scene_toolbar_layout.addLayout(far_hbox)
        self._scene_toolbar_layout.addLayout(samples_hbox)
        self._scene_toolbar_layout.addLayout(fr_hbox)
        self._scene_toolbar_layout.addLayout(loglevel_hbox)
        self._scene_toolbar_layout.addLayout(clearcolor_hbox)
        self._scene_toolbar_layout.addWidget(self.reload_btn)
        self._scene_toolbar_layout.addWidget(self._scn_view)

        self._scn_view.clicked.connect(self._scn_view_selected)
        self._scn_view.activated.connect(self._scn_view_selected)
        self._fps_chkbox.stateChanged.connect(self._fps_chkbox_changed)
        self._ar_cbbox.currentIndexChanged.connect(self._set_aspect_ratio)
        self._samples_cbbox.currentIndexChanged.connect(self._set_samples)
        self._fr_cbbox.currentIndexChanged.connect(self._set_frame_rate)
        self._loglevel_cbbox.currentIndexChanged.connect(self._set_loglevel)


class _MainWindow(QtWidgets.QSplitter):

    def _update_err_buf(self, err_str):
        if err_str:
            self._errbuf.setPlainText(err_str)
            self._errbuf.show()
            sys.stderr.write(err_str)
        else:
            self._errbuf.hide()

    @QtCore.pyqtSlot(str)
    def _all_scripts_err(self, err_str):
        self._scene_toolbar.clear_scripts()
        self._update_err_buf(err_str)

    def _get_scene(self, **cfg_overrides):
        cfg = self._scene_toolbar.get_scene_cfg()
        if cfg['scene'] is None:
            return None
        cfg['scene'] = (cfg['scene'][0], cfg['scene'][1])  # XXX
        cfg['glbackend'] = self._glbackend
        cfg['pkg'] = self._module_pkgname
        cfg['medias'] = self._medias
        cfg['system'] = platform.system()
        cfg.update(cfg_overrides)

        ret = query_subproc(query='scene', **cfg)
        if 'error' in ret:
            self._update_err_buf(ret['error'])
            return None
        else:
            self._update_err_buf(None)
            self._scripts_mgr.set_filelist(ret['filelist'])

        self._scene_toolbar.set_cfg(ret)

        return ret

    def _get_hook(self, name):
        if not self._hooksdir:
            return None
        hook = op.join(self._hooksdir, 'hook.' + name)
        if not op.exists(hook):
            return
        return hook

    def _get_hook_output(self, name):
        hook = self._get_hook(name)
        if not hook:
            return None
        return subprocess.check_output([hook]).rstrip()

    @QtCore.pyqtSlot(str, str)
    def _scene_changed(self, module_name, scene_name):
        self.setWindowTitle('%s - %s.%s' % (self._win_title_base, module_name, scene_name))
        self._currentTabChanged(self._tab_widget.currentIndex())

    @QtCore.pyqtSlot(str, str)
    def _scene_changed_hook(self, module_name, scene_name):

        def filename_escape(filename):
            s = ''
            for c in filename:
                cval = ord(c)
                if cval >= ord('!') and cval <= '~' and cval != '%':
                    s += c
                else:
                    s += '%%%02x' % (cval & 0xff)
            return s

        def get_remotefile(filename, remotedir):
            statinfo = os.stat(filename)
            sha256 = hashlib.sha256()
            sha256.update(filename)
            sha256.update(str(statinfo.st_size))
            sha256.update(str(statinfo.st_mtime))
            digest = sha256.hexdigest()
            _, ext = op.splitext(filename)
            return op.join(remotedir, digest + ext)

        try:
            # Bail out immediately if there is no script to run when a scene change
            # occurs
            hook_scene_change = self._get_hook('scene_change')
            if not hook_scene_change:
                return

            # The OpenGL backend can be different when using hooks: the scene might
            # be rendered on a remote device different from the one constructing
            # the scene graph
            glbackend = self._get_hook_output('get_gl_backend')
            system = self._get_hook_output('get_system')
            cfg = self._get_scene(glbackend=glbackend, system=system)
            if not cfg:
                return

            # The serialized scene is associated with a bunch of assets which we
            # need to sync. Similarly, the remote assets directory might be
            # different from the one in local, so we need to fix up the scene
            # appropriately.
            serialized_scene = cfg['scene']
            hook_sync = self._get_hook('media_sync')
            remotedir = self._get_hook_output('get_assets_dir')
            if hook_sync and remotedir:
                for media in cfg['medias']:
                    localfile = media.filename
                    remotefile = get_remotefile(media.filename, remotedir)
                    serialized_scene = serialized_scene.replace(
                            filename_escape(localfile),
                            filename_escape(remotefile))
                    subprocess.check_call([hook_sync, localfile, remotefile])

            # The serialized scene is then stored in a file which is then
            # communicated with additional parameters to the user
            local_scene = '/tmp/ngl_scene.ngl'
            open(local_scene, 'w').write(serialized_scene)
            args = [hook_scene_change, local_scene,
                    'duration=%f' % cfg['duration'],
                    'framerate=%d/%d' % cfg['framerate'],
                    'aspect_ratio=%d/%d' % cfg['aspect_ratio']]
            subprocess.check_call(args)

        except subprocess.CalledProcessError, e:
            QtWidgets.QMessageBox.critical(self, 'Hook error',
                                           'Error (%d) while executing %s' % (e.returncode, ' '.join(e.cmd)),
                                           QtWidgets.QMessageBox.Ok)

    def _emit_geometry(self):
        geometry = (self.x(), self.y(), self.width(), self.height())
        self._config.geometry_changed(geometry)

    @QtCore.pyqtSlot(QtGui.QResizeEvent)
    def resizeEvent(self, resize_event):
        super(_MainWindow, self).resizeEvent(resize_event)
        self._emit_geometry()

    @QtCore.pyqtSlot(QtGui.QMoveEvent)
    def moveEvent(self, move_event):
        super(_MainWindow, self).moveEvent(move_event)
        self._emit_geometry()

    @QtCore.pyqtSlot(int)
    def _currentTabChanged(self, index):
        view = self._tabs[index][1]
        if hasattr(view, 'refresh'):
            view.refresh()

    def __init__(self, module_pkgname, assets_dir, glbackend, hooksdir):
        super(_MainWindow, self).__init__(QtCore.Qt.Horizontal)
        self._win_title_base = 'Node.gl viewer'
        self.setWindowTitle(self._win_title_base)

        self._module_pkgname = module_pkgname
        self._glbackend = glbackend
        self._scripts_mgr = ScriptsManager(module_pkgname)
        self._hooksdir = hooksdir

        medias = None
        if assets_dir:
            medias = []
            for f in sorted(os.listdir(assets_dir)):
                ext = f.rsplit('.', 1)[-1].lower()
                path = op.join(assets_dir, f)
                if op.isfile(path) and ext in ('mp4', 'mkv', 'avi', 'webm', 'mov'):
                    try:
                        media = Media(path)
                    except:
                        pass
                    else:
                        medias.append(media)

        self._medias = medias

        get_scene_func = self._get_scene

        self._config = Config(module_pkgname)

        # Apply previous geometry (position + dimensions)
        rect = self._config.get('geometry')
        if rect:
            geometry = QtCore.QRect(*rect)
            self.setGeometry(geometry)

        gl_view = _GLView(get_scene_func, self._config)
        graph_view = _GraphView(get_scene_func)
        export_view = _ExportView(self, get_scene_func)
        serial_view = _SerialView(get_scene_func)

        self._tabs = [
            ('GL view', gl_view),
            ('Graph view', graph_view),
            ('Export', export_view),
            ('Serialization', serial_view),
        ]

        self._tab_widget = QtWidgets.QTabWidget()
        for tab_name, tab_view in self._tabs:
            self._tab_widget.addTab(tab_view, tab_name)
        self._tab_widget.currentChanged.connect(self._currentTabChanged)

        self._scene_toolbar = _Toolbar(self._config)
        self._scene_toolbar.sceneChanged.connect(self._scene_changed)
        self._scene_toolbar.sceneChanged.connect(self._scene_changed_hook)
        self._scene_toolbar.sceneChanged.connect(self._config.scene_changed)
        self._scene_toolbar.aspectRatioChanged.connect(gl_view.set_aspect_ratio)
        self._scene_toolbar.aspectRatioChanged.connect(self._config.set_aspect_ratio)
        self._scene_toolbar.samplesChanged.connect(gl_view.set_samples)
        self._scene_toolbar.samplesChanged.connect(self._config.set_samples)
        self._scene_toolbar.frameRateChanged.connect(gl_view.set_frame_rate)
        self._scene_toolbar.frameRateChanged.connect(self._config.set_frame_rate)
        self._scene_toolbar.logLevelChanged.connect(self._config.set_log_level)
        self._scene_toolbar.clearColorChanged.connect(gl_view.set_clear_color)
        self._scene_toolbar.clearColorChanged.connect(self._config.set_clear_color)

        self._errbuf = QtWidgets.QPlainTextEdit()
        self._errbuf.setFont(QtGui.QFontDatabase.systemFont(QtGui.QFontDatabase.FixedFont))
        self._errbuf.setReadOnly(True)
        self._errbuf.hide()

        tabs_and_errbuf = QtWidgets.QVBoxLayout()
        tabs_and_errbuf.addWidget(self._tab_widget)
        tabs_and_errbuf.addWidget(self._errbuf)
        tabs_and_errbuf_widget = QtWidgets.QWidget()
        tabs_and_errbuf_widget.setLayout(tabs_and_errbuf)

        self.addWidget(self._scene_toolbar)
        self.addWidget(tabs_and_errbuf_widget)
        self.setStretchFactor(1, 1)

        self._scene_toolbar.reload_btn.clicked.connect(self._scripts_mgr.reload)  # TODO: drop
        self._scripts_mgr.error.connect(self._all_scripts_err)
        self._scripts_mgr.scriptsChanged.connect(self._scene_toolbar.on_scripts_changed)
        self._scripts_mgr.start()

        # Load the previous scene if the current and previously loaded
        # module packages match
        prev_pkgname = self._config.get('pkg')
        prev_module = self._config.get('module')
        prev_scene = self._config.get('scene')
        if prev_pkgname == module_pkgname:
            self._scene_toolbar.load_scene_from_name(prev_module, prev_scene)


def run():
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('-m', dest='module', default='pynodegl_utils.examples',
                        help='set the module name containing the scene functions')
    parser.add_argument('-a', dest='assets_dir',
                        help='set the assets directory to be used by the scene functions')
    parser.add_argument('--gl-backend', dest='glbackend', choices=('gl', 'gles'), default='gl',
                        help='select the GL rendering backend')
    parser.add_argument('--hooks-dir', dest='hooksdir',
                        help='set the directory path containing event hooks')
    pargs = parser.parse_args(sys.argv[1:])

    QtGui.QSurfaceFormat.setDefaultFormat(get_gl_format(renderable=pargs.glbackend))

    app = QtWidgets.QApplication(sys.argv)
    window = _MainWindow(pargs.module, pargs.assets_dir, pargs.glbackend, pargs.hooksdir)
    window.show()
    app.exec_()
