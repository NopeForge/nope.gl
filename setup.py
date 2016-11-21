#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2016 Clément Bœsch <cboesch@gopro.com>
# Copyright 2016 Matthieu Bouron <mbouron@gopro.com>
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

import subprocess

from distutils.core import setup
from distutils.extension import Extension
from Cython.Build import cythonize

pkg_config_output = subprocess.check_output(['pkg-config', '--libs', '--cflags', 'libnodegl'])
flags = pkg_config_output.split()
include_dirs = [f[2:] for f in flags if f.startswith('-I')]
library_dirs = [f[2:] for f in flags if f.startswith('-L')]
libraries    = [f[2:] for f in flags if f.startswith('-l')]

def get_version():
    f = open('libnodegl.pc.tpl')
    ver_dict = {}
    version = None
    for line in f:
        if line.startswith('Version:'):
            version = line.split(':')[1].strip()
            break
    f.close()
    assert version is not None
    return version

setup(
    name = 'pynodegl',
    version = get_version(),
    ext_modules = cythonize([
        Extension("pynodegl", ["pynodegl.pyx"],
            include_dirs=include_dirs,
            libraries=libraries,
            library_dirs=library_dirs)
        ])
)
