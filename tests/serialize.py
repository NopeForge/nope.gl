#!/usr/bin/env python

import os
import os.path as op

from pynodegl_utils.com import query_subproc, query_inplace


def serialize(dirname, subproc=False):
    module_pkg = 'pynodegl_utils.examples'
    if subproc:
        ret = query_subproc(query='list', pkg=module_pkg)
    else:
        ret = query_inplace(query='list', pkg=module_pkg)
    assert 'error' not in ret
    scenes = ret['scenes']

    if not op.exists(dirname):
        os.makedirs(dirname)

    for module_name, sub_scenes in scenes:
        for scene_name, widgets_specs in sub_scenes:
            cfg = {
                'pkg': module_pkg,
                'scene': (module_name, scene_name),
            }
            if subproc:
                ret = query_subproc(query='scene', **cfg)
            else:
                ret = query_inplace(query='scene', **cfg)
            assert 'error' not in ret
            fname = op.join(dirname, '%s_%s.ngl' % (module_name, scene_name))
            print(fname)
            open(fname, 'w').write(ret['scene'])

if __name__ == '__main__':
    import sys
    serialize(op.join(sys.argv[1]))
