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

import importlib
import inspect
import os.path as op
import pkgutil
import traceback

from pynopegl_utils.module import load_script
from pynopegl_utils.resourcetracker import ResourceTracker


def _wrap_query(func):
    def wrapped_func(pkg, *args, **kwargs):
        module_is_script = pkg.endswith(".py")

        # Start tracking the imported modules and opened files
        rtracker = ResourceTracker()
        rtracker.start_hooking()

        odict = {}

        try:
            odict = func(pkg, *args, **kwargs)
        except Exception:
            odict["error"] = traceback.format_exc()

        # End of file and modules tracking
        rtracker.end_hooking()
        odict["filelist"] = rtracker.filelist
        odict["modulelist"] = rtracker.modulelist
        if module_is_script:
            odict["filelist"].update([pkg])

        return odict

    return wrapped_func


@_wrap_query
def query_scene(pkg, **idict):
    func = idict.pop("scene")

    # Call user constructing function
    extra_args = idict.pop("extra_args", {})
    odict = func(idict, **extra_args)
    odict["scene"].root.set_label(func.__name__)
    return odict


@_wrap_query
def query_list(pkg):
    module_is_script = pkg.endswith(".py")

    scripts = []

    # Import the script, or the package and its sub-modules
    if module_is_script:
        pkg = op.realpath(pkg)
        module = load_script(pkg)
        scripts.append((module.__name__, module))
    else:
        module = importlib.import_module(pkg)
        module_is_package = hasattr(module, "__path__")
        if module_is_package:
            for submod in pkgutil.iter_modules(module.__path__):
                _, module_name, ispkg = submod
                if ispkg:
                    continue
                script = importlib.import_module("." + module_name, pkg)
                import_name = f"{pkg}.{module_name}"
                scripts.append((import_name, script))
        else:
            scripts.append((module.__name__, module))

    # Find all the scenes
    scenes = []
    for module_name, script in scripts:
        all_funcs = inspect.getmembers(script, callable)
        sub_scenes = []
        for func in all_funcs:
            scene_name, func_wrapper = func
            if not hasattr(func_wrapper, "iam_a_ngl_scene_func"):
                continue
            sub_scenes.append((scene_name, func_wrapper))
        if sub_scenes:
            scenes.append((module_name, sub_scenes))

    return dict(scenes=scenes)
