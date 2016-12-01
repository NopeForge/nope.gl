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

from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext

class LibNodeGLConfig:

    PKG_LIB_NAME = 'libnodegl'

    def __init__(self, pkg_config_bin='pkg-config'):
        import subprocess

        if subprocess.call([pkg_config_bin, '--exists', self.PKG_LIB_NAME]) != 0:
            raise Exception('%s is required to build pynodegl' % self.PKG_LIB_NAME)

        self.version       = subprocess.check_output([pkg_config_bin, '--modversion', self.PKG_LIB_NAME]).strip()
        self.data_root_dir = subprocess.check_output([pkg_config_bin, '--variable=datarootdir', self.PKG_LIB_NAME]).strip()
        pkgcfg_libs_cflags = subprocess.check_output([pkg_config_bin, '--libs', '--cflags', self.PKG_LIB_NAME])

        flags = pkgcfg_libs_cflags.split()
        self.include_dirs = [f[2:] for f in flags if f.startswith('-I')]
        self.library_dirs = [f[2:] for f in flags if f.startswith('-L')]
        self.libraries    = [f[2:] for f in flags if f.startswith('-l')]

lib_cfg = LibNodeGLConfig()

class BuildExtCommand(build_ext):

    def _gen_definitions_pyx(self, specs):
        import yaml # must NOT be on top of this file

        specs = yaml.load(open(specs))

        _get_struct_name = lambda name: 'ngl_%s' % name.lower()

        def _get_vec_init_code(n, vecname, cvecname):
            return '''
        cdef float[%(n)d] %(cvecname)s
        cdef int %(vecname)s_i
        for %(vecname)s_i in range(%(n)d):
            %(cvecname)s[%(vecname)s_i] = %(vecname)s[%(vecname)s_i]
''' % {'n': n, 'vecname': vecname, 'cvecname': cvecname}

        content = 'from libc.stdlib cimport free\n'
        content += 'cdef extern from "nodegl.h":\n'

        nodes_decls = []
        for item in specs:
            node = item.keys()[0]
            fields = item[node]
            nodes_decls.append('cdef int NGL_NODE_%s' % node.upper())
        nodes_decls.append(None)

        content += '\n'.join(('    %s' % d) if d else '' for d in nodes_decls) + '\n'

        for item in specs:
            node = item.keys()[0]
            fields = item[node]
            sname = _get_struct_name(node)

            type_id = 'NGL_NODE_%s' % node.upper()
            construct_args = ['self']
            construct_cargs = [type_id]
            special_inits = ''
            extra_args = ''

            if not fields:
                fields = {}

            for field in fields.get('constructors', []):
                field_name, field_type = field
                assert not field_type.endswith('List')
                if field_type in ('int', 'float', 'double'):
                    construct_cargs.append(field_name)
                    construct_args.append('%s %s' % (field_type, field_name))
                elif field_type == 'string':
                    construct_cargs.append(field_name)
                    construct_args.append('const char *%s' % field_name)
                elif field_type.startswith('vec'):
                    n = int(field_type[3:])
                    cparam = field_name + '_c'
                    special_inits += _get_vec_init_code(n, field_name, cparam)
                    construct_cargs.append(cparam)
                    construct_args.append(field_name)
                else:
                    construct_cargs.append('%s.ctx' % field_name)
                    construct_args.append('_Node %s' % field_name)

            opt_fields = fields.get('optional', [])
            if node != '_Node':
                opt_fields += specs[0]['_Node']['optional']

            for field in opt_fields:
                field_name, field_type = field
                construct_args.append('%s=None' % field_name)
                is_list = field_type.endswith('List')
                dereference = is_list or field_type.startswith('vec')
                optset_data = {
                    'var': field_name,
                    'op': 'add' if is_list else 'set',
                    'arg': '*' + field_name if dereference else field_name,
                }
                extra_args += '''
        if %(var)s is not None:
            self.%(op)s_%(var)s(%(arg)s)''' % optset_data

            class_data = {
                'class_name': node,
                'struct_name': _get_struct_name(node),
                'construct_args': ', '.join(construct_args),
                'construct_cargs': ', '.join(construct_cargs),
                'special_inits': special_inits,
                'extra_args': extra_args,
            }

            class_str = '''
cdef class %(class_name)s(_Node):
    def __cinit__(%(construct_args)s):%(special_inits)s
        self.ctx = ngl_node_create(%(construct_cargs)s)
        if self.ctx is NULL:
            raise MemoryError()
%(extra_args)s
''' % class_data

            if node == '_Node':
                class_str = '''
cdef class _Node:
    cdef ngl_node *ctx

    def dot(self):
        cdef char *s
        s = ngl_node_dot(self.ctx)
        try:
            pystr = <bytes>s
        finally:
            free(s)
        return pystr

    def __dealloc__(self):
        ngl_node_unrefp(&self.ctx)
'''

            for field in fields.get('optional', []):
                field_name, field_type = field

                if field_type.endswith('List'):
                    field_name, field_type = field
                    assert field_type == 'NodeList' # TODO support other lists
                    ctype = field_type
                    cparam = field_name
                    field_data = {
                        'field_name': field_name,
                        'field_type': ctype,
                        'cparam': cparam,
                        'citem_type': 'ngl_node *',
                        'clist': field_name + '_c'
                    }
                    class_str += '''
    def add_%(field_name)s(self, *%(field_name)s):
        %(clist)s = <%(citem_type)s*>calloc(len(%(field_name)s), sizeof(%(citem_type)s))
        if %(clist)s is NULL:
            raise MemoryError()
        for i, item in enumerate(%(field_name)s):
            %(clist)s[i] = (<_Node>item).ctx

        return ngl_node_param_add(self.ctx, "%(field_name)s",
                                  len(%(field_name)s), %(clist)s)
''' % field_data

                elif field_type.startswith('vec'):
                    n = int(field_type[3:])
                    cparam = field_name + '_c'
                    field_data = {
                        'field_name': field_name,
                        'cparam': cparam,
                        'vec_init_code': _get_vec_init_code(n, field_name, cparam),
                    }
                    class_str += '''
    def set_%(field_name)s(self, *%(field_name)s):%(vec_init_code)s
        return ngl_node_param_set(self.ctx, "%(field_name)s", %(cparam)s)
''' % field_data

                else:
                    ctype = field_type
                    cparam = field_name
                    if field_type == 'string':
                        ctype = 'const char *'
                    elif field_type == 'Node':
                        ctype = '_Node'
                        cparam += '.ctx'
                    field_data = {
                        'field_name': field_name,
                        'field_type': ctype,
                        'cparam': cparam,
                    }
                    class_str += '''
    def set_%(field_name)s(self, %(field_type)s %(field_name)s):
        return ngl_node_param_set(self.ctx, "%(field_name)s", %(cparam)s)
''' % field_data

            content += class_str + '\n'

        return content

    def run(self):
        import os
        specs_file = os.path.join(lib_cfg.data_root_dir, 'nodegl', 'nodes.specs')
        content = self._gen_definitions_pyx(specs_file)
        open('nodes_def.pyx', 'w').write(content)
        build_ext.run(self)

setup(
    name='pynodegl',
    version=lib_cfg.version,
    setup_requires=[
        'setuptools>=18.0',
        'cython',
        'pyyaml',
    ],
    cmdclass={
        'build_ext': BuildExtCommand,
    },
    ext_modules=[Extension("pynodegl",
                 sources=['pynodegl.pyx'],
                 include_dirs=lib_cfg.include_dirs,
                 libraries=lib_cfg.libraries,
                 library_dirs=lib_cfg.library_dirs)]
)
