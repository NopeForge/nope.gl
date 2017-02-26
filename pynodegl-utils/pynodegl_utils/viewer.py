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
import math
import examples
import importlib
import inspect
import json
import pkgutil
import platform
import subprocess
import traceback

import pynodegl as ngl
from export import Exporter

from PyQt5 import QtGui, QtCore, QtWidgets


ASPECT_RATIOS = [(16, 9), (16, 10), (4, 3), (1, 1)]

class _GLWidget(QtWidgets.QOpenGLWidget):

    def set_time(self, t):
        self._time = t
        self.update()

    def set_scene(self, scene):
        self._viewer.set_scene(scene)
        self.update()

    def set_aspect_ratio(self, aspect_ratio):
        self._aspect_ratio = aspect_ratio
        # XXX: self.resize(self.size()) doesn't seem to have any effect as it
        # doesn't call resizeGL() callback, so we do something a bit more
        # clumsy
        self.makeCurrent()
        self.resizeGL(self.width(), self.height())
        self.doneCurrent()
        self.update()

    def __init__(self, parent, aspect_ratio):
        super(_GLWidget, self).__init__(parent)

        gl_format = QtGui.QSurfaceFormat()
        gl_format.setVersion(3, 3)
        gl_format.setProfile(QtGui.QSurfaceFormat.CoreProfile)
        gl_format.setDepthBufferSize(24);
        gl_format.setStencilBufferSize(8);
        gl_format.setAlphaBufferSize(8);
        self.setFormat(gl_format)

        self.setMinimumSize(640, 360)
        self._viewer = ngl.Viewer()
        self._time = 0
        self._aspect_ratio = aspect_ratio
        self.resizeGL(self.width(), self.height())

    def paintGL(self):
        self._viewer.set_viewport(self.view_x, self.view_y, self.view_width, self.view_height)
        self._viewer.draw(self._time)

    def resizeGL(self, screen_width, screen_height):
        aspect = self._aspect_ratio[0] / float(self._aspect_ratio[1])
        self.view_width = screen_width
        self.view_height = screen_width / aspect

        if self.view_height > screen_height:
            self.view_height = screen_height
            self.view_width = screen_height * aspect

        self.view_x = (screen_width - self.view_width) / 2.0
        self.view_y = (screen_height - self.view_height) / 2.0

    def initializeGL(self):
        if platform.system() == 'Linux':
            self._viewer.configure(ngl.GLPLATFORM_GLX, ngl.GLAPI_OPENGL3)
        elif platform.system() == 'Darwin':
            self._viewer.configure(ngl.GLPLATFORM_CGL, ngl.GLAPI_OPENGL3)


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
        scene = self._get_scene_func()
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
                            self.parent.LOOP_DURATION, fps,
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
    def _update_graph(self):
        scene = self._get_scene_func()
        if not scene:
            return

        dotfile = '/tmp/ngl_scene.dot'
        open(dotfile, 'w').write(scene.dot())
        data = subprocess.check_output(['dot', '-Tpng', dotfile])
        pixmap = QtGui.QPixmap()
        pixmap.loadFromData(data)
        self._graph_lbl.setPixmap(pixmap)
        self._graph_lbl.adjustSize()

    def __init__(self, get_scene_func):
        super(_GraphView, self).__init__()

        self._get_scene_func = get_scene_func

        self._graph_btn = QtWidgets.QPushButton("Update Graph")
        self._graph_lbl = QtWidgets.QLabel()
        img_area = QtWidgets.QScrollArea()
        img_area.setWidget(self._graph_lbl)

        graph_layout = QtWidgets.QVBoxLayout(self)
        graph_layout.addWidget(self._graph_btn)
        graph_layout.addWidget(img_area)

        self._graph_btn.clicked.connect(self._update_graph)


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
        if t > self._scene_cfg.duration:
            self._tick = 0
        cur_time = '%02d:%02d' % divmod(self._tick, 60)
        duration = '%02d:%02d' % divmod(self._scene_cfg.duration, 60)
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

    @QtCore.pyqtSlot()
    def scene_changed(self):
        scene = self._get_scene_func()
        if scene:
            self._gl_widget.set_scene(scene)

    def __init__(self, get_scene_func, default_ar, scene_cfg):
        super(_GLView, self).__init__()

        self._get_scene_func = get_scene_func

        self._scene_cfg = scene_cfg

        self._timer = QtCore.QTimer()
        self._timer.setInterval(1000.0 / self.RENDERING_FPS) # in milliseconds

        self._gl_widget = _GLWidget(self, default_ar)

        self._slider = QtWidgets.QSlider(QtCore.Qt.Horizontal)
        self._slider.setRange(0, scene_cfg.duration * self.RENDERING_FPS)

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

        gl_layout = QtWidgets.QVBoxLayout(self)
        gl_layout.addWidget(self._gl_widget)
        gl_layout.setStretchFactor(self._gl_widget, 1)
        gl_layout.addWidget(self._slider)
        gl_layout.addLayout(toolbar)

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

    def construct_current_scene(self):
        if not self._current_scene_data:
            return None
        module_name, scene_name, scene_func = self._current_scene_data
        scene = scene_func(self._scene_cfg, **self._scene_extra_args)
        scene.set_name(scene_name)

        if self._fps_chkbox.isChecked():
            from pynodegl import FPS, Quad, Shader, Texture, TexturedShape, Group
            fps = FPS(scene, measure_update=1, measure_draw=1, create_databuf=1)
            q = Quad((0, 15/16., 0), (1., 0, 0), (0, 1/16., 0))
            s = Shader()
            t = Texture(data_src=fps)
            tshape = TexturedShape(q, s, t)
            g = Group()
            g.add_children(fps, tshape)
            scene = g

        return scene

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
    def _scn_view_clicked(self, index):
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
        self._scene_cfg.aspect_ratio = ar[0] / float(ar[1])
        self.aspectRatioChanged.emit(ar)
        self._load_current_scene()

    def __init__(self, scene_cfg, module_pkgname, default_ar):
        super(_Toolbar, self).__init__()

        self._module_pkgname = module_pkgname

        self._scene_opts_widget = None
        self._scene_extra_args = {}

        self._scene_cfg = scene_cfg

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

        self._loglevel_cbbox = QtWidgets.QComboBox()
        for level in self.LOG_LEVELS:
            self._loglevel_cbbox.addItem(level.title())
        self._loglevel_cbbox.setCurrentIndex(self.LOG_LEVELS.index('debug'))
        self._set_loglevel()
        loglevel_lbl = QtWidgets.QLabel('Min log level:')
        loglevel_hbox = QtWidgets.QHBoxLayout()
        loglevel_hbox.addWidget(loglevel_lbl)
        loglevel_hbox.addWidget(self._loglevel_cbbox)

        self.reload_btn = QtWidgets.QPushButton('Reload scripts')

        self._scene_toolbar_layout = QtWidgets.QVBoxLayout(self)
        self._scene_toolbar_layout.addWidget(self._fps_chkbox)
        self._scene_toolbar_layout.addLayout(ar_hbox)
        self._scene_toolbar_layout.addLayout(loglevel_hbox)
        self._scene_toolbar_layout.addWidget(self.reload_btn)
        self._scene_toolbar_layout.addWidget(self._scn_view)

        self._scn_view.clicked.connect(self._scn_view_clicked)
        self._fps_chkbox.stateChanged.connect(self._fps_chkbox_changed)
        self._ar_cbbox.currentIndexChanged.connect(self._set_aspect_ratio)
        self._loglevel_cbbox.currentIndexChanged.connect(self._set_loglevel)


class _ScriptsManager(QtCore.QObject):

    scripts_changed = QtCore.pyqtSignal(list, name='scriptsChanged')
    error = QtCore.pyqtSignal(str)

    def __init__(self, module_pkgname):
        super(_ScriptsManager, self).__init__()
        self._module_pkgname = module_pkgname

    def start(self):
        self._reload_scripts(initial_import=True)

    @QtCore.pyqtSlot()
    def reload(self): # TODO: should be called automatically
        self._reload_scripts()

    def _reload_unsafe(self, initial_import):
        scripts = []

        if initial_import:
            self._module = importlib.import_module(self._module_pkgname)
        else:
            self._module = reload(self._module)

        for module in pkgutil.iter_modules(self._module.__path__):
            module_finder, module_name, ispkg = module
            script = importlib.import_module('.' + module_name, self._module_pkgname)
            if not initial_import:
                reload(script)
            scripts.append((module_name, script))

        self.scripts_changed.emit(scripts)

    def _reload_scripts(self, initial_import=False):
        try:
            self._reload_unsafe(initial_import)
        except:
            self.error.emit(traceback.format_exc())


class _MainWindow(QtWidgets.QSplitter):

    LOOP_DURATION = 30.0
    DEFAULT_MEDIA_FILE = '/tmp/ngl-media.mkv'

    def _get_media_dimensions(self, filename):
        try:
            data = subprocess.check_output(['ffprobe', '-v', '0',
                                            '-select_streams', 'v:0',
                                            '-of', 'json', '-show_streams',
                                            filename])
            data = json.loads(data)
        except:
            return (-1, -1)
        st = data['streams'][0]
        return (st['width'], st['height'])

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

    def _get_scene(self):
        scene = None
        try:
            scene = self._scene_toolbar.construct_current_scene()
        except:
            self._update_err_buf(traceback.format_exc())
        else:
            self._update_err_buf(None)
        finally:
            return scene

    def __init__(self, args):
        super(_MainWindow, self).__init__(QtCore.Qt.Horizontal)
        self.setWindowTitle("Node.gl viewer")

        if not args:
            module_pkgname = 'pynodegl_utils.examples'
        else:
            module_pkgname = args[0]
            args = args[1:]
        self._scripts_mgr = _ScriptsManager(module_pkgname)

        if not args:
            media_file = self.DEFAULT_MEDIA_FILE
            if not os.path.exists(self.DEFAULT_MEDIA_FILE):
                ret = subprocess.call(['ffmpeg', '-nostdin', '-nostats', '-f', 'lavfi', '-i',
                                       'testsrc2=d=%d:r=%d' % (int(math.ceil(self.LOOP_DURATION)), _GLView.RENDERING_FPS),
                                       media_file])
                if ret:
                    raise Exception("Unable to create a media file using ffmpeg (ret=%d)" % ret)
        else:
            media_file = args[0]

        default_ar = ASPECT_RATIOS[0]

        class _SceneCfg: pass
        self._scene_cfg = _SceneCfg()
        self._scene_cfg.media_filename = media_file
        self._scene_cfg.media_dimensions = self._get_media_dimensions(media_file)
        self._scene_cfg.duration = self.LOOP_DURATION
        self._scene_cfg.aspect_ratio = default_ar[0] / float(default_ar[1])

        get_scene_func = self._get_scene

        gl_view = _GLView(get_scene_func, default_ar, self._scene_cfg)
        graph_view = _GraphView(get_scene_func)
        export_view = _ExportView(self, get_scene_func)

        tabs = QtWidgets.QTabWidget()
        tabs.addTab(gl_view, "GL view")
        tabs.addTab(graph_view, "Graph view")
        tabs.addTab(export_view, "Export")

        self._scene_toolbar = _Toolbar(self._scene_cfg, module_pkgname, default_ar)
        self._scene_toolbar.sceneChanged.connect(gl_view.scene_changed)
        self._scene_toolbar.aspectRatioChanged.connect(gl_view.set_aspect_ratio)

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
    app = QtWidgets.QApplication(sys.argv)
    window = _MainWindow(sys.argv[1:])
    window.show()
    app.exec_()
