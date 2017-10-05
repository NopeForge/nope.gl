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

import os
import sys
import imp
import importlib
import inspect
import pkgutil
import subprocess
import traceback
import distutils
import __builtin__

from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler

import pynodegl as ngl

from misc import NGLMedia, NGLSceneCfg
from export import Exporter
from gl import get_gl_format

from PyQt5 import QtGui, QtCore, QtWidgets
from OpenGL import GL


ASPECT_RATIOS = [(16, 9), (16, 10), (4, 3), (1, 1)]
SAMPLES = [0, 2, 4, 8]

class _SerialView(QtWidgets.QWidget):

    @QtCore.pyqtSlot()
    def _save_to_file(self):
        data = self._text.toPlainText()
        filenames = QtWidgets.QFileDialog.getSaveFileName(self, 'Select export file')
        if not filenames[0]:
            return
        open(filenames[0], 'w').write(data)

    @QtCore.pyqtSlot()
    def _update_graph(self):
        scene, _ = self._get_scene_func()
        if not scene:
            QtWidgets.QMessageBox.critical(self, 'No scene',
                                           "You didn't select any scene to graph.",
                                           QtWidgets.QMessageBox.Ok)
            return
        self._text.setPlainText(scene.serialize())

    @QtCore.pyqtSlot()
    def scene_changed(self):
        if self._auto_chkbox.isChecked():
            self._update_graph()

    @QtCore.pyqtSlot(int)
    def _auto_check_changed(self, state):
        if state:
            self._buffer_btn.setEnabled(False)
        else:
            self._buffer_btn.setEnabled(True)

    def __init__(self, get_scene_func):
        super(_SerialView, self).__init__()

        self._get_scene_func = get_scene_func

        self._buffer_btn = QtWidgets.QPushButton("Update buffer")
        self._save_btn = QtWidgets.QPushButton("Save to file")
        self._auto_chkbox = QtWidgets.QCheckBox("Automatically update")
        self._graph_lbl = QtWidgets.QLabel()
        self._text = QtWidgets.QPlainTextEdit()

        hbox = QtWidgets.QHBoxLayout()
        hbox.addWidget(self._buffer_btn)
        hbox.addWidget(self._save_btn)

        serial_layout = QtWidgets.QVBoxLayout(self)
        serial_layout.addWidget(self._auto_chkbox)
        serial_layout.addLayout(hbox)
        serial_layout.addWidget(self._text)

        self._buffer_btn.clicked.connect(self._update_graph)
        self._save_btn.clicked.connect(self._save_to_file)
        self._auto_chkbox.stateChanged.connect(self._auto_check_changed)


class _GLWidget(QtWidgets.QOpenGLWidget):

    def get_time(self):
        return self._time

    def set_time(self, t):
        self._time = t
        self.update()

    def set_scene(self, scene, cfg):
        self._viewer.set_glstates(*cfg.glstates)
        self._viewer.set_scene(scene)
        self.update()

    def get_aspect_ratio(self):
        return self._aspect_ratio

    def set_aspect_ratio(self, aspect_ratio):
        self._aspect_ratio = aspect_ratio
        # XXX: self.resize(self.size()) doesn't seem to have any effect as it
        # doesn't call resizeGL() callback, so we do something a bit more
        # clumsy
        self.makeCurrent()
        self.resizeGL(self.width(), self.height())
        self.doneCurrent()
        self.update()

    def __init__(self, parent, aspect_ratio, samples):
        super(_GLWidget, self).__init__(parent)

        self.setMinimumSize(640, 360)
        self._viewer = ngl.Viewer()
        self._time = 0
        self._aspect_ratio = aspect_ratio
        self._samples = samples
        self.resizeGL(self.width(), self.height())

    def paintGL(self):
        GL.glViewport(self.view_x, self.view_y, self.view_width, self.view_height)
        self._viewer.draw(self._time)

    def resizeGL(self, screen_width, screen_height):
        screen_width = int(screen_width * self.devicePixelRatioF())
        screen_height = int(screen_height * self.devicePixelRatioF())
        aspect = self._aspect_ratio[0] / float(self._aspect_ratio[1])
        self.view_width = screen_width
        self.view_height = int(screen_width / aspect)

        if self.view_height > screen_height:
            self.view_height = screen_height
            self.view_width = int(screen_height * aspect)

        self.view_x = int((screen_width - self.view_width) / 2.0)
        self.view_y = int((screen_height - self.view_height) / 2.0)

    def initializeGL(self):
        api = ngl.GLAPI_OPENGL3
        if self.context().isOpenGLES():
            api = ngl.GLAPI_OPENGLES2
        GL.glClearColor(0.0, 0.0, 0.0, 1.0)
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
        scene, cfg = self._get_scene_func()
        if scene is not None:
            ofile  = self._ofile_text.text()
            width  = self._spinbox_width.value()
            height = self._spinbox_height.value()
            fps    = self._spinbox_fps.value()

            extra_enc_args = []
            encoder_id = self._encoders_cbox.currentIndex()
            if encoder_id:
                extra_enc_args += ['-c:v', self._encoders_cbox.itemText(encoder_id)]
            extra_enc_args += self._encopts_text.text().split()

            self._pgbar.setValue(0)
            self._pgbar.show()

            exporter = Exporter()
            exporter.progressed.connect(self._pgbar.setValue)
            exporter.export(scene, ofile, width, height,
                            cfg.duration, fps,
                            cfg.glstates,
                            extra_enc_args)

            self._pgbar.hide()
        else:
            QtWidgets.QMessageBox.critical(self, 'No scene',
                                           "You didn't select any scene to export.",
                                           QtWidgets.QMessageBox.Ok)

    @QtCore.pyqtSlot()
    def _select_ofile(self):
        filenames = QtWidgets.QFileDialog.getSaveFileName(self, 'Select export file')
        if not filenames[0]:
            return
        self._ofile_text.setText(filenames[0])

    def __init__(self, parent, get_scene_func):
        super(_ExportView, self).__init__()

        self._get_scene_func = get_scene_func
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

        self._spinbox_fps = QtWidgets.QSpinBox()
        self._spinbox_fps.setRange(1, 1000)
        self._spinbox_fps.setValue(60)

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
        form.addRow('FPS:',      self._spinbox_fps)
        form.addRow('Encoder:',  self._encoders_cbox)
        form.addRow('Extra encoder arguments:', self._encopts_text)
        form.addRow(self._export_btn)
        form.addRow(self._pgbar)

        ofile_btn.clicked.connect(self._select_ofile)
        self._export_btn.clicked.connect(self._export)


class _GraphView(QtWidgets.QWidget):

    @QtCore.pyqtSlot()
    def _save_to_file(self):
        pixmap = self._graph_lbl.pixmap()
        if not pixmap:
            return
        img = pixmap.toImage()
        filenames = QtWidgets.QFileDialog.getSaveFileName(self, 'Select export file')
        if not filenames[0]:
            return
        img.save(filenames[0])

    @QtCore.pyqtSlot()
    def _update_graph(self):
        scene, _ = self._get_scene_func()
        if not scene:
            QtWidgets.QMessageBox.critical(self, 'No scene',
                                           "You didn't select any scene to graph.",
                                           QtWidgets.QMessageBox.Ok)
            return

        dotfile = '/tmp/ngl_scene.dot'
        open(dotfile, 'w').write(scene.dot())
        try:
            data = subprocess.check_output(['dot', '-Tpng', dotfile])
        except OSError, e:
            QtWidgets.QMessageBox.critical(self, 'Graphviz error',
                                          'Error while executing dot (Graphviz): %s' % e.strerror,
                                           QtWidgets.QMessageBox.Ok)
            return

        pixmap = QtGui.QPixmap()
        pixmap.loadFromData(data)
        self._graph_lbl.setPixmap(pixmap)
        self._graph_lbl.adjustSize()

    @QtCore.pyqtSlot()
    def scene_changed(self):
        if self._auto_chkbox.isChecked():
            self._update_graph()

    @QtCore.pyqtSlot(int)
    def _auto_check_changed(self, state):
        if state:
            self._graph_btn.setEnabled(False)
        else:
            self._graph_btn.setEnabled(True)

    def __init__(self, get_scene_func):
        super(_GraphView, self).__init__()

        self._get_scene_func = get_scene_func

        self._graph_btn = QtWidgets.QPushButton("Update Graph")
        self._save_btn = QtWidgets.QPushButton("Save to file")
        self._auto_chkbox = QtWidgets.QCheckBox("Automatically update")
        self._graph_lbl = QtWidgets.QLabel()
        img_area = QtWidgets.QScrollArea()
        img_area.setWidget(self._graph_lbl)

        hbox = QtWidgets.QHBoxLayout()
        hbox.addWidget(self._graph_btn)
        hbox.addWidget(self._save_btn)

        graph_layout = QtWidgets.QVBoxLayout(self)
        graph_layout.addWidget(self._auto_chkbox)
        graph_layout.addLayout(hbox)
        graph_layout.addWidget(img_area)

        self._graph_btn.clicked.connect(self._update_graph)
        self._save_btn.clicked.connect(self._save_to_file)
        self._auto_chkbox.stateChanged.connect(self._auto_check_changed)


class _GLView(QtWidgets.QWidget):

    RENDERING_FPS = 60

    def _set_action(self, action):
        if action == 'play':
            self._timer.start()
            self._action_btn.setText(u'❙❙')
        elif action == 'pause':
            self._timer.stop()
            self._action_btn.setText(u'►')
        self._current_action = action

    @QtCore.pyqtSlot()
    def _slider_clicked(self):
        self._set_action('pause')

    @QtCore.pyqtSlot()
    def _toggle_playback(self):
        self._set_action('play' if self._current_action == 'pause' else 'pause')

    def _update_tick(self, value):
        self._tick = value
        t = self._tick * 1./self.RENDERING_FPS
        if t > self._scene_duration:
            self._tick = 0
        cur_time = '%02d:%02d' % divmod(t, 60)
        duration = '%02d:%02d' % divmod(self._scene_duration, 60)
        self._time_lbl.setText('%s / %s (%d @ %dHz)' % (cur_time, duration, self._tick, self.RENDERING_FPS))
        self._slider.setValue(self._tick)

    @QtCore.pyqtSlot(int)
    def _slider_value_changed(self, value):
        if self._gl_widget:
            self._gl_widget.set_time(value * 1./self.RENDERING_FPS)
        self._update_tick(value)

    @QtCore.pyqtSlot()
    def _increment_time(self):
        self._update_tick(self._tick + 1)

    @QtCore.pyqtSlot()
    def _step_fw(self):
        self._set_action('pause')
        self._update_tick(self._tick + 1)

    @QtCore.pyqtSlot()
    def _step_bw(self):
        self._set_action('pause')
        self._update_tick(self._tick - 1)

    @QtCore.pyqtSlot()
    def _screenshot(self):
        filenames = QtWidgets.QFileDialog.getSaveFileName(self, 'Save screenshot file')
        if not filenames[0]:
            return
        self._gl_widget.grabFramebuffer().save(filenames[0])

    @QtCore.pyqtSlot(tuple)
    def set_aspect_ratio(self, ar):
        self._gl_widget.set_aspect_ratio(ar)

    @QtCore.pyqtSlot(int)
    def _set_samples(self, samples):
        gl_widget = _GLWidget(self, self._default_ar, self._default_samples)
        gl_widget.set_time(self._gl_widget.get_time())
        gl_widget.set_aspect_ratio(self._gl_widget.get_aspect_ratio())
        self._gl_layout.replaceWidget(self._gl_widget, gl_widget)
        self._gl_widget = gl_widget

    @QtCore.pyqtSlot()
    def scene_changed(self):
        scene, cfg = self._get_scene_func()
        if scene:
            self._gl_widget.set_scene(scene, cfg)
            self._scene_duration = cfg.duration
            self._slider.setRange(0, self._scene_duration * self.RENDERING_FPS)
            self._update_tick(self._tick)

    def __init__(self, get_scene_func, default_ar, default_samples):
        super(_GLView, self).__init__()

        self._default_ar = default_ar
        self._default_samples = default_samples

        self._get_scene_func = get_scene_func

        self._scene_duration = 0

        self._timer = QtCore.QTimer()
        self._timer.setInterval(1000.0 / self.RENDERING_FPS) # in milliseconds

        self._gl_widget = _GLWidget(self, default_ar, default_samples)

        self._slider = QtWidgets.QSlider(QtCore.Qt.Horizontal)

        self._action_btn = QtWidgets.QPushButton()
        self._set_action('pause')

        fw_btn = QtWidgets.QPushButton('>')
        bw_btn = QtWidgets.QPushButton('<')

        self._time_lbl = QtWidgets.QLabel()

        screenshot_btn = QtWidgets.QPushButton('Screenshot')

        toolbar = QtWidgets.QHBoxLayout()
        toolbar.addWidget(bw_btn)
        toolbar.addWidget(self._action_btn)
        toolbar.addWidget(fw_btn)
        toolbar.addWidget(self._time_lbl)
        toolbar.setStretchFactor(self._time_lbl, 1)
        toolbar.addWidget(screenshot_btn)

        self._gl_layout = QtWidgets.QVBoxLayout(self)
        self._gl_layout.addWidget(self._gl_widget)
        self._gl_layout.setStretchFactor(self._gl_widget, 1)
        self._gl_layout.addWidget(self._slider)
        self._gl_layout.addLayout(toolbar)

        self._update_tick(0)

        self._timer.timeout.connect(self._increment_time)
        self._action_btn.clicked.connect(self._toggle_playback)
        fw_btn.clicked.connect(self._step_fw)
        bw_btn.clicked.connect(self._step_bw)
        self._slider.sliderPressed.connect(self._slider_clicked)
        self._slider.valueChanged.connect(self._slider_value_changed)
        screenshot_btn.clicked.connect(self._screenshot)


class _Toolbar(QtWidgets.QWidget):

    LOG_LEVELS = ('verbose', 'debug', 'info', 'warning', 'error')

    scene_changed = QtCore.pyqtSignal(name='sceneChanged')
    aspect_ratio_changed = QtCore.pyqtSignal(tuple, name='aspectRatioChanged')
    samples_changed = QtCore.pyqtSignal(int, name='samplesChanged')

    def _del_scene_opts_widget(self):
        if self._scene_opts_widget:
            self._scene_toolbar_layout.removeWidget(self._scene_opts_widget)
            self._scene_opts_widget.deleteLater()
            self._scene_opts_widget = None

    def _set_scene_opts_widget(self, widget):
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
            label.setText(self._get_label_text(id_name, os.path.basename(filenames[0])))
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

            elif widget_specs['type'] == 'model':
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
        if not self._current_scene_data:
            return None
        module_name, scene_name, scene_func = self._current_scene_data
        return {
                'name': scene_name,
                'func': scene_func,
                'aspect_ratio': ASPECT_RATIOS[self._ar_cbbox.currentIndex()],
                'extra_args': self._scene_extra_args,
                'has_fps': self._fps_chkbox.isChecked(),
        }

    def _load_current_scene(self, load_widgets=True):
        if not self._current_scene_data:
            return
        module_name, scene_name, scene_func = self._current_scene_data
        if load_widgets:
            self._scene_extra_args = {}
            self._del_scene_opts_widget()
            scene_opts_widget = self._get_opts_widget_from_specs(scene_func.widgets_specs)
            self._set_scene_opts_widget(scene_opts_widget)
        self.sceneChanged.emit()

    def clear_scripts(self):
        self._scn_mdl.clear()

    def _reload_scene_view(self, scenes):
        self._scn_mdl.clear()
        for module_name, scene_funcs in scenes:
            qitem_script = QtGui.QStandardItem(module_name)
            for scene_name, scene_func in scene_funcs:
                scene_data = (module_name, scene_name, scene_func)
                qitem_func = QtGui.QStandardItem(scene_name)
                qitem_script.appendRow([qitem_func])
                qitem_func.setData(scene_data)
            self._scn_mdl.appendRow(qitem_script)
            index = self._scn_mdl.indexFromItem(qitem_script)
        self._scn_view.expandAll()

    @QtCore.pyqtSlot(QtCore.QModelIndex)
    def _scn_view_selected(self, index):
        data = self._scn_mdl.itemFromIndex(index).data()
        if data:
            self._current_scene_data = data
            self._load_current_scene()

    @QtCore.pyqtSlot(list)
    def on_scripts_changed(self, scripts):
        found_current_scene = False
        scenes = []

        for module_name, script in scripts:
            all_funcs = inspect.getmembers(script, inspect.isfunction)
            scene_funcs = filter(lambda f: hasattr(f[1], 'iam_a_ngl_scene_func'), all_funcs)
            if not scene_funcs:
                continue
            scenes.append((module_name, scene_funcs))

            # found back the current scene in the module if needed
            if not found_current_scene and self._current_scene_data:
                cur_module_name, cur_scene_name, cur_scene_func = self._current_scene_data
                if module_name == cur_module_name:
                    matches = filter(lambda f: f[0] == cur_scene_name, scene_funcs)
                    if matches:
                        assert len(matches) == 1
                        scene_name, scene_func = matches[0]
                        self._current_scene_data = (cur_module_name, scene_name, scene_func)
                        found_current_scene = True

        self._reload_scene_view(scenes)
        self._load_current_scene()

    @QtCore.pyqtSlot()
    def _fps_chkbox_changed(self):
        self._load_current_scene()

    @QtCore.pyqtSlot()
    def _set_loglevel(self):
        level_id = self._loglevel_cbbox.currentIndex()
        level_str = self.LOG_LEVELS[level_id]
        ngl_level = eval('ngl.LOG_%s' % level_str.upper())
        ngl.log_set_min_level(ngl_level)

    @QtCore.pyqtSlot()
    def _set_aspect_ratio(self):
        ar = ASPECT_RATIOS[self._ar_cbbox.currentIndex()]
        self.aspectRatioChanged.emit(ar)
        self._load_current_scene()

    @QtCore.pyqtSlot()
    def _set_samples(self):
        samples = SAMPLES[self._samples_cbbox.currentIndex()]

        gl_format = QtGui.QSurfaceFormat.defaultFormat()
        gl_format.setSamples(samples)
        QtGui.QSurfaceFormat.setDefaultFormat(gl_format)

        self.samplesChanged.emit(samples)
        self._load_current_scene()

    def __init__(self, default_ar, default_samples):
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

        self._ar_cbbox = QtWidgets.QComboBox()
        for ar in ASPECT_RATIOS:
            self._ar_cbbox.addItem('%d:%d' % ar)
        self._ar_cbbox.setCurrentIndex(ASPECT_RATIOS.index(default_ar))
        self._set_aspect_ratio()
        ar_lbl = QtWidgets.QLabel('Aspect ratio:')
        ar_hbox = QtWidgets.QHBoxLayout()
        ar_hbox.addWidget(ar_lbl)
        ar_hbox.addWidget(self._ar_cbbox)

        self._samples_cbbox = QtWidgets.QComboBox()
        for samples in SAMPLES:
            self._samples_cbbox.addItem('%dx' % samples if samples else 'Disabled')
        self._samples_cbbox.setCurrentIndex(SAMPLES.index(default_samples))
        samples_lbl = QtWidgets.QLabel('MSAA:')
        samples_hbox = QtWidgets.QHBoxLayout()
        samples_hbox.addWidget(samples_lbl)
        samples_hbox.addWidget(self._samples_cbbox)

        self._loglevel_cbbox = QtWidgets.QComboBox()
        for level in self.LOG_LEVELS:
            self._loglevel_cbbox.addItem(level.title())
        self._loglevel_cbbox.setCurrentIndex(self.LOG_LEVELS.index('debug'))
        self._set_loglevel()
        loglevel_lbl = QtWidgets.QLabel('Min log level:')
        loglevel_hbox = QtWidgets.QHBoxLayout()
        loglevel_hbox.addWidget(loglevel_lbl)
        loglevel_hbox.addWidget(self._loglevel_cbbox)

        self.reload_btn = QtWidgets.QPushButton('Force scripts reload')

        self._scene_toolbar_layout = QtWidgets.QVBoxLayout(self)
        self._scene_toolbar_layout.addWidget(self._fps_chkbox)
        self._scene_toolbar_layout.addLayout(ar_hbox)
        self._scene_toolbar_layout.addLayout(samples_hbox)
        self._scene_toolbar_layout.addLayout(loglevel_hbox)
        self._scene_toolbar_layout.addWidget(self.reload_btn)
        self._scene_toolbar_layout.addWidget(self._scn_view)

        self._scn_view.clicked.connect(self._scn_view_selected)
        self._scn_view.activated.connect(self._scn_view_selected)
        self._fps_chkbox.stateChanged.connect(self._fps_chkbox_changed)
        self._ar_cbbox.currentIndexChanged.connect(self._set_aspect_ratio)
        self._samples_cbbox.currentIndexChanged.connect(self._set_samples)
        self._loglevel_cbbox.currentIndexChanged.connect(self._set_loglevel)


class _ScriptsManager(QtCore.QObject):

    MODULES_BLACKLIST = [
        'numpy',
        'threading',
        'watchdog',
    ]

    scripts_changed = QtCore.pyqtSignal(list, name='scriptsChanged')
    error = QtCore.pyqtSignal(str)

    def __init__(self, module_pkgname):
        super(_ScriptsManager, self).__init__()
        self._module_is_script = module_pkgname.endswith('.py')
        self._module_pkgname = module_pkgname
        if self._module_is_script:
            self._module_pkgname = os.path.realpath(self._module_pkgname)
        self._builtin_import = __builtin__.__import__
        self._builtin_open = __builtin__.open
        self._dirs_to_watch = set()
        self._files_to_watch = set()
        self._modules_to_reload = set()
        self._pysysdir = os.path.realpath(distutils.sysconfig.get_python_lib(standard_lib=True))

        self._event_handler = FileSystemEventHandler()
        self._event_handler.on_any_event = self._on_any_event
        self._observer = Observer()
        self._observer.start()

    def _mod_is_blacklisted(self, mod):
        if not hasattr(mod, '__file__'):
            return True

        modpath = os.path.realpath(os.path.dirname(mod.__file__))
        if modpath == self._pysysdir:
            return True

        modname = mod.__name__
        for bl_mod in self.MODULES_BLACKLIST:
            if modname.startswith(bl_mod):
                return True

        return False

    def start(self):
        self._reload_scripts(initial_import=True)

    @QtCore.pyqtSlot()
    def reload(self):
        self._reload_scripts()

    def _on_any_event(self, event):
        if event.src_path in self._files_to_watch:
            print('Change in %s detected, reload scene' % event.src_path)
            self.reload()

    def start_hooking(self):
        __builtin__.__import__ = self._import_hook
        __builtin__.open = self._open_hook

    def end_hooking(self):
        __builtin__.__import__ = self._builtin_import
        __builtin__.open = self._builtin_open

        self._observer.unschedule_all()
        for path in self._dirs_to_watch:
            self._observer.schedule(self._event_handler, path)

    def _queue_watch_path(self, path):
        self._dirs_to_watch.update([os.path.dirname(path)])
        if path.endswith('.pyc'):
            path = path[:-1]
        self._files_to_watch.update([path])

    def _load_script(self, path):
        dname = os.path.dirname(path)
        fname = os.path.basename(path)
        name = fname[:-3]
        fp, pathname, description = imp.find_module(name, [dname])
        try:
            return imp.load_module(name, fp, pathname, description)
        finally:
            if fp:
                fp.close()

    def _reload_unsafe(self, initial_import):

        modules_to_reload = self._modules_to_reload.copy()
        for i, module in enumerate(modules_to_reload):
            reload(module)

        if initial_import:
            if self._module_is_script:
                self._module = self._load_script(self._module_pkgname)
            else:
                self._module = importlib.import_module(self._module_pkgname)
            self._queue_watch_path(self._module.__file__)

        scripts = []
        if self._module_is_script:
            if not initial_import:
                self._module = self._load_script(self._module_pkgname) # reload
            self._queue_watch_path(self._module_pkgname)
            scripts.append((self._module.__name__, self._module))
        else:
            for module in pkgutil.iter_modules(self._module.__path__):
                module_finder, module_name, ispkg = module
                script = importlib.import_module('.' + module_name, self._module_pkgname)
                if not initial_import:
                    reload(script)
                self._queue_watch_path(script.__file__)
                scripts.append((module_name, script))

        self.scripts_changed.emit(scripts)

    def _import_hook(self, name, globals={}, locals={}, fromlist=[], level=-1):
        ret = self._builtin_import(name, globals, locals, fromlist, level)
        if not self._mod_is_blacklisted(ret):
            self._queue_watch_path(ret.__file__)
            self._modules_to_reload.update([ret])
        return ret

    def _open_hook(self, name, mode="r", buffering=-1):
        ret = self._builtin_open(name, mode, buffering)
        self._queue_watch_path(name)
        return ret

    def _reload_scripts(self, initial_import=False):
        self.start_hooking()
        try:
            self._reload_unsafe(initial_import)
        except:
            self.error.emit(traceback.format_exc())
        self.end_hooking()


class _MainWindow(QtWidgets.QSplitter):

    def _update_err_buf(self, err_str):
        if err_str:
            self._errbuf.setText(err_str)
            self._errbuf.show()
            sys.stderr.write(err_str)
        else:
            self._errbuf.hide()

    @QtCore.pyqtSlot(str)
    def _all_scripts_err(self, err_str):
        self._scene_toolbar.clear_scripts()
        self._update_err_buf(err_str)

    def _construct_current_scene(self, cfg_dict):
        ar = cfg_dict['aspect_ratio']

        scene_cfg = NGLSceneCfg(medias=self._medias)
        scene_cfg.aspect_ratio = ar[0] / float(ar[1])
        scene_cfg.glbackend = self._glbackend

        scene = cfg_dict['func'](scene_cfg, **cfg_dict['extra_args'])
        scene.set_name(cfg_dict['name'])

        if cfg_dict['has_fps']:
            from pynodegl import FPS, Quad, Program, Texture, Render, Group
            fps = FPS(scene, measure_update=1, measure_draw=1, create_databuf=1)
            q = Quad((0, 15/16., 0), (1., 0, 0), (0, 1/16., 0))
            p = Program()
            t = Texture(data_src=fps)
            render = Render(q, p)
            render.update_textures(tex0=t)
            g = Group()
            g.add_children(fps, render)
            scene = g

        return scene, scene_cfg

    def _get_scene(self):
        cfg_dict = self._scene_toolbar.get_scene_cfg()
        scene = None
        cfg = None
        if not cfg_dict:
            return None, None
        try:
            self._scripts_mgr.start_hooking()
            scene, cfg = self._construct_current_scene(cfg_dict)
        except:
            self._update_err_buf(traceback.format_exc())
        else:
            self._update_err_buf(None)
        finally:
            self._scripts_mgr.end_hooking()
            return scene, cfg

    def __init__(self, module_pkgname, assets_dir, glbackend):
        super(_MainWindow, self).__init__(QtCore.Qt.Horizontal)
        self.setWindowTitle("Node.gl viewer")

        self._glbackend = glbackend
        self._scripts_mgr = _ScriptsManager(module_pkgname)

        medias = None
        if assets_dir:
            medias = []
            for f in sorted(os.listdir(assets_dir)):
                ext = f.rsplit('.', 1)[-1].lower()
                path = os.path.join(assets_dir, f)
                if os.path.isfile(path) and ext in ('mp4', 'mkv', 'avi', 'webm', 'mov'):
                    try:
                        media = NGLMedia(path)
                    except:
                        pass
                    else:
                        medias.append(media)

        self._medias = medias

        default_ar = ASPECT_RATIOS[0]
        default_samples = SAMPLES[0]

        get_scene_func = self._get_scene

        gl_view = _GLView(get_scene_func, default_ar, default_samples)
        graph_view = _GraphView(get_scene_func)
        export_view = _ExportView(self, get_scene_func)
        serial_view = _SerialView(get_scene_func)

        tabs = QtWidgets.QTabWidget()
        tabs.addTab(gl_view, "GL view")
        tabs.addTab(graph_view, "Graph view")
        tabs.addTab(export_view, "Export")
        tabs.addTab(serial_view, "Serialization")

        self._scene_toolbar = _Toolbar(default_ar, default_samples)
        self._scene_toolbar.sceneChanged.connect(gl_view.scene_changed)
        self._scene_toolbar.sceneChanged.connect(graph_view.scene_changed)
        self._scene_toolbar.sceneChanged.connect(serial_view.scene_changed)
        self._scene_toolbar.aspectRatioChanged.connect(gl_view.set_aspect_ratio)
        self._scene_toolbar.samplesChanged.connect(gl_view._set_samples)

        self._errbuf = QtWidgets.QTextEdit()
        self._errbuf.hide()

        tabs_and_errbuf = QtWidgets.QVBoxLayout()
        tabs_and_errbuf.addWidget(tabs)
        tabs_and_errbuf.addWidget(self._errbuf)
        tabs_and_errbuf_widget = QtWidgets.QWidget()
        tabs_and_errbuf_widget.setLayout(tabs_and_errbuf)

        self.addWidget(self._scene_toolbar)
        self.addWidget(tabs_and_errbuf_widget)
        self.setStretchFactor(1, 1)

        self._scene_toolbar.reload_btn.clicked.connect(self._scripts_mgr.reload) # TODO: drop
        self._scripts_mgr.error.connect(self._all_scripts_err)
        self._scripts_mgr.scriptsChanged.connect(self._scene_toolbar.on_scripts_changed)
        self._scripts_mgr.start()


def run():
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('-m', dest='module', default='pynodegl_utils.examples',
                        help='set the module name containing the scene functions')
    parser.add_argument('-a', dest='assets_dir',
                        help='set the assets directory to be used by the scene functions')
    parser.add_argument('--gl-backend', dest='glbackend', choices=('gl', 'gles'), default='gl',
                        help='select the GL rendering backend')
    pargs = parser.parse_args(sys.argv[1:])

    QtGui.QSurfaceFormat.setDefaultFormat(get_gl_format(renderable=pargs.glbackend))

    app = QtWidgets.QApplication(sys.argv)
    window = _MainWindow(pargs.module, pargs.assets_dir, pargs.glbackend)
    window.show()
    app.exec_()
