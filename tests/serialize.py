#!/usr/bin/env python

import os
import math
import importlib
import inspect
import pkgutil
import random
import subprocess

def serialize(dirname):
    from pynodegl_utils import examples
    from pynodegl_utils.misc import NGLMedia, NGLSceneCfg

    scripts = []
    for module in pkgutil.iter_modules(examples.__path__):
        module_finder, module_name, ispkg = module
        script = importlib.import_module('.' + module_name, 'pynodegl_utils.examples')
        scripts.append((module_name, script))

    scenes = []
    for module_name, script in scripts:
        all_funcs = inspect.getmembers(script, inspect.isfunction)
        scene_funcs = filter(lambda f: hasattr(f[1], 'iam_a_ngl_scene_func'), all_funcs)
        if not scene_funcs:
            continue
        scenes.append((module_name, scene_funcs))

    scene_cfg = NGLSceneCfg()
    scene_cfg.aspect_ratio = (16, 9)

    try:
        os.makedirs(dirname)
    except OSError:
        pass
    for module_name, scene_funcs in scenes:
        scene_cfg.duration = 30.0
        for scene_name, scene_func in scene_funcs:
            random.seed(0x67891234)
            scene = scene_func(scene_cfg)
            data = scene.serialize()
            fname = os.path.join(dirname, '%s_%s.ngl' % (module_name, scene_name))
            print(fname)
            open(fname, 'w').write(data)

if __name__ == '__main__':
    import sys
    serialize(os.path.join(sys.argv[1]))
