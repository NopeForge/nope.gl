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
import pkgutil
import subprocess
import traceback

import pynodegl as ngl

from PySide import QtGui, QtCore
from PySide.QtOpenGL import QGLWidget

ASPECT_RATIO = (16, 9)

class _GLWidget(QGLWidget):

    def set_time(self, t):
        self._time = t
        self.update()

    def set_scene(self, scene):
        self._scene = scene
        self.update()

    def __init__(self, parent):
        QGLWidget.__init__(self, parent)
        self.setMinimumSize(640, 360)
        self._viewer = ngl.Viewer(None)
        self._scene = None
        self._time = 0

    def paintGL(self):
        if self._scene:
            self._viewer.draw(self._scene, self._time)

    def resizeGL(self, screen_width, screen_height):
        aspect = ASPECT_RATIO[0] / float(ASPECT_RATIO[1])
        view_width = screen_width
        view_height = screen_width / aspect

        if view_height > screen_height:
            view_height = screen_height
            view_width = screen_height * aspect

        view_x = (screen_width - view_width) / 2.0
        view_y = (screen_height - view_height) / 2.0

        self._viewer.set_viewport(view_x, view_y, view_width, view_height)

    def initializeGL(self):
        self._viewer.set_window(ngl.GLPLATFORM_GLX, ngl.GLAPI_OPENGL3)

class _MainWindow(QtGui.QSplitter):

    RENDERING_FPS = 60
    DEFAULT_MEDIA_FILE = '/tmp/ngl-media.mkv'
    LOOP_DURATION = 30.0
    LOG_LEVELS = ('verbose', 'debug', 'info', 'warning', 'error')

    def _set_action(self, action):
        if action == 'play':
            self._timer.start()
            self._action_btn.setText(u'❙❙')
        elif action == 'pause':
            self._timer.stop()
            self._action_btn.setText(u'►')
        self._current_action = action

    def _slider_clicked(self):
        self._set_action('pause')

    def _toggle_playback(self):
        self._set_action('play' if self._current_action == 'pause' else 'pause')

    def _update_tick(self, value):
        self._tick = value
        t = self._tick * 1./self.RENDERING_FPS
        if t > self.LOOP_DURATION:
            self._tick = 0
        cur_time = '%02d:%02d' % divmod(self._tick, 60)
        duration = '%02d:%02d' % divmod(self.LOOP_DURATION, 60)
        self._time_lbl.setText('%s / %s (%d @ %dHz)' % (cur_time, duration, self._tick, self.RENDERING_FPS))
        self._slider.setValue(self._tick)

    def _slider_value_changed(self, value):
        if self._gl_widget:
            self._gl_widget.set_time(value * 1./self.RENDERING_FPS)
        self._update_tick(value)

    def _increment_time(self):
        self._update_tick(self._tick + 1)

    def _step_fw(self):
        self._set_action('pause')
        self._update_tick(self._tick + 1)

    def _step_bw(self):
        self._set_action('pause')
        self._update_tick(self._tick - 1)

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
        def slider_value_changed(value):
            real_value = value if unit_base is 1 else value / float(unit_base)
            self._scene_extra_args[id_name] = real_value
            label.setText(self._get_label_text(id_name, real_value))
            self._load_current_scene(load_widgets=False)
        return slider_value_changed

    def _craft_pick_color_cb(self, id_name, label, color_btn):
        def pick_color():
            color = QtGui.QColorDialog.getColor()
            color_btn.setStyleSheet('background-color: %s;' % color.name())
            label.setText(self._get_label_text(id_name, color.name()))
            self._scene_extra_args[id_name] = color.getRgbF()
            self._load_current_scene(load_widgets=False)
        return pick_color

    def _craft_choose_filename_cb(self, id_name, label, dialog_btn, files_filter):
        def choose_filename():
            filenames = QtGui.QFileDialog.getOpenFileName(self, 'Open file', '', files_filter)
            if not filenames[0]:
                return
            label.setText(self._get_label_text(id_name, os.path.basename(filenames[0])))
            self._scene_extra_args[id_name] = filenames[0]
            self._load_current_scene(load_widgets=False)
        return choose_filename

    def _craft_checkbox_toggle_cb(self, id_name, chkbox):
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
                slider = QtGui.QSlider(QtCore.Qt.Horizontal)
                unit_base = widget_specs.get('unit_base', 1)
                if 'range' in widget_specs:
                    srange = widget_specs['range']
                    slider.setRange(srange[0] * unit_base, srange[1] * unit_base)
                slider.setValue(default_value * unit_base)
                label = QtGui.QLabel(self._get_label_text(name, default_value))
                widgets.append(label)
                slider_value_changed_cb = self._craft_slider_value_changed_cb(name, label, unit_base)
                slider.valueChanged.connect(slider_value_changed_cb)
                widgets.append(slider)

            elif widget_specs['type'] == 'color':
                color_btn = QtGui.QPushButton()
                color = QtGui.QColor()
                color.setRgbF(*default_value)
                color_name = color.name()
                color_btn.setStyleSheet('background-color: %s;' % color_name)
                label = QtGui.QLabel(self._get_label_text(name, color_name))
                widgets.append(label)
                pick_color_cb = self._craft_pick_color_cb(name, label, color_btn)
                color_btn.pressed.connect(pick_color_cb)
                widgets.append(color_btn)

            elif widget_specs['type'] == 'bool':
                chkbox = QtGui.QCheckBox(name)
                chkbox.setChecked(default_value)
                checkbox_toggle_cb = self._craft_checkbox_toggle_cb(name, chkbox)
                chkbox.stateChanged.connect(checkbox_toggle_cb)
                widgets.append(chkbox)

            elif widget_specs['type'] == 'model':
                dialog_btn = QtGui.QPushButton('Open file')
                label = QtGui.QLabel(self._get_label_text(name, default_value))
                widgets.append(label)
                choose_filename_cb = self._craft_choose_filename_cb(name, label, dialog_btn, widget_specs.get('filter', ''))
                dialog_btn.pressed.connect(choose_filename_cb)
                widgets.append(dialog_btn)

        if not widgets:
            return None

        groupbox = QtGui.QGroupBox('Custom scene options')
        vbox = QtGui.QVBoxLayout()
        for widget in widgets:
            vbox.addWidget(widget)
        groupbox.setLayout(vbox)
        return groupbox

    def _load_current_scene(self, load_widgets=True):
        if not self._current_scene_data:
            return
        module_name, scene_name, scene_func = self._current_scene_data
        if load_widgets:
            self._scene_extra_args = {}
            self._del_scene_opts_widget()
            scene_opts_widget = self._get_opts_widget_from_specs(scene_func.widgets_specs)
            self._set_scene_opts_widget(scene_opts_widget)
        try:
            scene = scene_func(self._scene_cfg, **self._scene_extra_args)
            scene.set_name(scene_name)
        except:
            self._errbuf.setText(traceback.format_exc())
            self._errbuf.show()
            raise
        else:
            self._errbuf.hide()

        self._base_scene = scene
        self._reload_scene()

    def _reload_scene_view(self):
        self._scn_mdl.clear()
        for module_name, scene_funcs in self._scenes:
            qitem_script = QtGui.QStandardItem(module_name)
            for scene_name, scene_func in scene_funcs:
                scene_data = (module_name, scene_name, scene_func)
                qitem_func = QtGui.QStandardItem(scene_name)
                qitem_script.appendRow([qitem_func])
                qitem_func.setData(scene_data)
            self._scn_mdl.appendRow(qitem_script)
            index = self._scn_mdl.indexFromItem(qitem_script)
        self._scn_view.expandAll()

    def _scn_view_clicked(self, index):
        self._current_scene_data = self._scn_mdl.itemFromIndex(index).data()
        self._load_current_scene()

    def _reload_scripts(self, initial_import=False):
        self._scenes = []
        found_current_scene = False

        if initial_import:
            self._module = importlib.import_module(self._module_pkgname)
        else:
            self._module = reload(self._module)

        for module in pkgutil.iter_modules(self._module.__path__):

            # load or reload scripts
            module_finder, module_name, ispkg = module
            script = importlib.import_module('.' + module_name, self._module_pkgname)
            if not initial_import:
                try:
                    reload(script)
                except:
                    self._errbuf.setText(traceback.format_exc())
                    self._errbuf.show()
                    raise
                else:
                    self._errbuf.hide()

            all_funcs = inspect.getmembers(script, inspect.isfunction)
            scene_funcs = filter(lambda f: hasattr(f[1], 'iam_a_ngl_scene_func'), all_funcs)
            if not scene_funcs:
                continue
            self._scenes.append((module_name, scene_funcs))

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
                        self._load_current_scene()

        self._reload_scene_view()

    def _reload_scene(self):
        scene = self._base_scene
        if not scene:
            return

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

        self._scene = scene
        self._gl_widget.set_scene(scene)

    def _update_graph(self):
        scene = self._scene
        if not scene:
            return

        dotfile = '/tmp/ngl_scene.dot'
        open(dotfile, 'w').write(scene.dot())
        data = subprocess.check_output(['dot', '-Tpng', dotfile])
        pixmap = QtGui.QPixmap()
        pixmap.loadFromData(data)
        self._graph_lbl.setPixmap(pixmap)
        self._graph_lbl.adjustSize()

    def _set_loglevel(self):
        level_id = self._loglevel_cbbox.currentIndex()
        level_str = self.LOG_LEVELS[level_id]
        ngl_level = eval('ngl.LOG_%s' % level_str.upper())
        ngl.log_set_min_level(ngl_level)

    def __init__(self, args):
        super(_MainWindow, self).__init__(QtCore.Qt.Horizontal)
        self.setWindowTitle("Demo node.gl")

        if not args:
            self._module_pkgname = 'pynodegl_utils.examples'
        else:
            self._module_pkgname = args[0]
            args = args[1:]

        if not args:
            media_file = self.DEFAULT_MEDIA_FILE
            if not os.path.exists(self.DEFAULT_MEDIA_FILE):
                ret = subprocess.call(['ffmpeg', '-nostdin', '-nostats', '-f', 'lavfi', '-i',
                                       'testsrc2=d=%d:r=%d' % (int(math.ceil(self.LOOP_DURATION)), self.RENDERING_FPS),
                                       media_file])
                if ret:
                    raise Exception("Unable to create a media file using ffmpeg (ret=%d)" % ret)
        else:
            media_file = args[0]

        class _SceneCfg: pass
        self._scene_cfg = _SceneCfg()
        self._scene_cfg.media_filename = media_file
        self._scene_cfg.duration = self.LOOP_DURATION
        self._scene_cfg.aspect_ratio = ASPECT_RATIO[0] / float(ASPECT_RATIO[1])

        self._gl_widget = _GLWidget(self)
        self._scene = None
        self._base_scene = None
        self._scene_opts_widget = None
        self._scene_extra_args = {}

        self._timer = QtCore.QTimer()
        self._timer.setInterval(1000.0 / self.RENDERING_FPS) # in milliseconds

        self._slider = QtGui.QSlider(QtCore.Qt.Horizontal, self)
        self._slider.setRange(0, self.LOOP_DURATION * self.RENDERING_FPS)

        self._action_btn = QtGui.QPushButton()
        self._set_action('pause')

        fw_btn = QtGui.QPushButton('>')
        bw_btn = QtGui.QPushButton('<')

        self._time_lbl = QtGui.QLabel()

        toolbar = QtGui.QHBoxLayout()
        toolbar.addWidget(bw_btn)
        toolbar.addWidget(self._action_btn)
        toolbar.addWidget(fw_btn)
        toolbar.addWidget(self._time_lbl)
        toolbar.setStretchFactor(self._time_lbl, 1)

        self._graph_btn = QtGui.QPushButton("Update Graph")
        self._graph_lbl = QtGui.QLabel()
        img_area = QtGui.QScrollArea()
        img_area.setWidget(self._graph_lbl)
        graph_layout = QtGui.QVBoxLayout()
        graph_layout.addWidget(self._graph_btn)
        graph_layout.addWidget(img_area)
        graph_tab_widget = QtGui.QWidget()
        graph_tab_widget.setLayout(graph_layout)

        gl_layout = QtGui.QVBoxLayout()
        gl_layout.addWidget(self._gl_widget)
        gl_layout.setStretchFactor(self._gl_widget, 1)
        gl_layout.addWidget(self._slider)
        gl_layout.addLayout(toolbar)
        gl_tab_widget = QtGui.QWidget()
        gl_tab_widget.setLayout(gl_layout)

        tabs = QtGui.QTabWidget()
        tabs.addTab(gl_tab_widget, "GL view")
        tabs.addTab(graph_tab_widget, "Graph view")

        self._scn_view = QtGui.QTreeView()
        self._scn_view.setHeaderHidden(True)
        self._scn_view.setSelectionBehavior(QtGui.QAbstractItemView.SelectRows)
        self._scn_view.setEditTriggers(QtGui.QAbstractItemView.NoEditTriggers)
        self._scn_mdl = QtGui.QStandardItemModel()
        self._scn_view.setModel(self._scn_mdl)

        self._current_scene_data = None
        self._reload_scripts(initial_import=True)
        self._reload_scene_view()

        self._loglevel_cbbox = QtGui.QComboBox()
        for level in self.LOG_LEVELS:
            self._loglevel_cbbox.addItem(level.title())
        self._loglevel_cbbox.setCurrentIndex(self.LOG_LEVELS.index('debug'))
        self._set_loglevel()
        loglevel_lbl = QtGui.QLabel('Min log level:')
        loglevel_hbox = QtGui.QHBoxLayout()
        loglevel_hbox.addWidget(loglevel_lbl)
        loglevel_hbox.addWidget(self._loglevel_cbbox)

        self._fps_chkbox = QtGui.QCheckBox('Show FPS')
        reload_btn = QtGui.QPushButton('Reload scripts')

        self._scene_toolbar_layout = QtGui.QVBoxLayout()
        self._scene_toolbar_layout.addWidget(self._fps_chkbox)
        self._scene_toolbar_layout.addLayout(loglevel_hbox)
        self._scene_toolbar_layout.addWidget(reload_btn)
        self._scene_toolbar_layout.addWidget(self._scn_view)

        scene_toolbar = QtGui.QWidget()
        scene_toolbar.setLayout(self._scene_toolbar_layout)

        self._errbuf = QtGui.QTextEdit()
        self._errbuf.hide()

        tabs_and_errbuf = QtGui.QVBoxLayout()
        tabs_and_errbuf.addWidget(tabs)
        tabs_and_errbuf.addWidget(self._errbuf)
        tabs_and_errbuf_widget = QtGui.QWidget()
        tabs_and_errbuf_widget.setLayout(tabs_and_errbuf)

        self.addWidget(scene_toolbar)
        self.addWidget(tabs_and_errbuf_widget)
        self.setStretchFactor(1, 1)

        self._update_tick(0)

        self._timer.timeout.connect(self._increment_time)
        self._action_btn.clicked.connect(self._toggle_playback)
        fw_btn.clicked.connect(self._step_fw)
        bw_btn.clicked.connect(self._step_bw)
        self._slider.sliderPressed.connect(self._slider_clicked)
        self._slider.valueChanged.connect(self._slider_value_changed)
        self._scn_view.clicked.connect(self._scn_view_clicked)
        self._graph_btn.clicked.connect(self._update_graph)
        self._fps_chkbox.stateChanged.connect(self._reload_scene)
        self._loglevel_cbbox.currentIndexChanged.connect(self._set_loglevel)
        reload_btn.clicked.connect(self._reload_scripts)

    def closeEvent(self, event):
        del self._gl_widget
        self._gl_widget = None

def run():
    app = QtGui.QApplication(sys.argv)
    window = _MainWindow(sys.argv[1:])
    window.show()
    app.exec_()
