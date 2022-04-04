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

import importlib
import inspect
import os.path as op
import pkgutil
import traceback
from pynodegl_utils.module import load_script
from pynodegl_utils.resourcetracker import ResourceTracker


def query_scene(pkg, **idict):
    module_is_script = pkg.endswith('.py')

    # Start tracking the imported modules and opened files
    rtracker = ResourceTracker()
    rtracker.start_hooking()

    odict = {}

    try:
        # Get module.func
        module_name, scene_name = idict['scene']
        if module_is_script:
            module = load_script(pkg)
        else:
            import_name = f'{pkg}.{module_name}'
            module = importlib.import_module(import_name)
        func = getattr(module, scene_name)

        # Call user constructing function
        odict = func(idict, **idict.get('extra_args', {}))
        scene = odict['scene']
        del odict['scene']
        scene.set_label(scene_name)

        # Prepare output data
        odict['scene'] = scene.dot() if idict.get('fmt') == 'dot' else scene.serialize()

    except:
        odict = {'error': traceback.format_exc()}

    # End of file and modules tracking
    rtracker.end_hooking()
    odict['filelist'] = rtracker.filelist
    odict['modulelist'] = rtracker.modulelist
    if module_is_script:
        odict['filelist'].update([pkg])

    return odict


def query_list(pkg):
    module_is_script = pkg.endswith('.py')

    # Start tracking the imported modules and opened files
    rtracker = ResourceTracker()
    rtracker.start_hooking()

    odict = {}

    try:
        scripts = []

        # Import the script, or the package and its sub-modules
        if module_is_script:
            pkg = op.realpath(pkg)
            module = load_script(pkg)
            scripts.append((module.__name__, module))
        else:
            module = importlib.import_module(pkg)
            for submod in pkgutil.iter_modules(module.__path__):
                module_finder, module_name, ispkg = submod
                if ispkg:
                    continue
                script = importlib.import_module('.' + module_name, pkg)
                scripts.append((module_name, script))

        # Find all the scenes
        scenes = []
        for module_name, script in scripts:
            all_funcs = inspect.getmembers(script, inspect.isfunction)
            sub_scenes = []
            for func in all_funcs:
                scene_name, func_wrapper = func
                if not hasattr(func_wrapper, 'iam_a_ngl_scene_func'):
                    continue
                sub_scenes.append((scene_name, func_wrapper.__doc__, func_wrapper.widgets_specs))
            if sub_scenes:
                scenes.append((module_name, sub_scenes))

        # Prepare output data
        odict['scenes'] = scenes

    except:
        odict = {'error': traceback.format_exc()}

    # End of file and modules tracking
    rtracker.end_hooking()
    odict['filelist'] = rtracker.filelist
    odict['modulelist'] = rtracker.modulelist
    if module_is_script:
        odict['filelist'].update([pkg])

    return odict
