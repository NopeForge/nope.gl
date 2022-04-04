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

import pprint

from setuptools import Command, Extension, find_packages, setup
from setuptools.command.build_ext import build_ext


class LibNodeGLConfig:

    PKG_LIB_NAME = "libnodegl"

    def __init__(self, pkg_config_bin="pkg-config"):
        import subprocess

        if subprocess.call([pkg_config_bin, "--exists", self.PKG_LIB_NAME]) != 0:
            raise Exception(f"{self.PKG_LIB_NAME} is required to build pynodegl")

        self.version = subprocess.check_output([pkg_config_bin, "--modversion", self.PKG_LIB_NAME]).strip().decode()
        self.data_root_dir = (
            subprocess.check_output([pkg_config_bin, "--variable=datarootdir", self.PKG_LIB_NAME]).strip().decode()
        )
        pkgcfg_libs_cflags = subprocess.check_output([pkg_config_bin, "--libs", "--cflags", self.PKG_LIB_NAME]).decode()

        flags = pkgcfg_libs_cflags.split()
        self.include_dirs = [f[2:] for f in flags if f.startswith("-I")]
        self.library_dirs = [f[2:] for f in flags if f.startswith("-L")]
        self.libraries = [f[2:] for f in flags if f.startswith("-l")]


_LIB_CFG = LibNodeGLConfig()


class CommandUtils:
    @staticmethod
    def _gen_specs_py(specs):
        specs_dump = pprint.pformat(specs, sort_dicts=False)
        return f"SPECS = {specs_dump}\n"

    @staticmethod
    def _gen_definitions_pyx(specs):
        # Map C nodes identifiers (NGL_NODE_*)
        content = 'cdef extern from "nodegl.h":\n'
        nodes_decls = []
        constants = []
        for node in specs.keys():
            if not node.startswith("_"):
                name = f"NODE_{node.upper()}"
                constants.append(name)
                nodes_decls.append(f"cdef int NGL_{name}")
        nodes_decls.append(None)
        content += "\n".join((f"    {d}") if d else "" for d in nodes_decls) + "\n"
        content += "\n".join(f"{name} = NGL_{name}" for name in constants) + "\n"
        return content

    @staticmethod
    def write_definitions_pyx():
        import os.path as op

        import yaml  # must NOT be on top of this file

        specs_file = op.join(_LIB_CFG.data_root_dir, "nodegl", "nodes.specs")
        with open(specs_file) as f:
            specs = yaml.safe_load(f)
        content = CommandUtils._gen_definitions_pyx(specs)
        with open("nodes_def.pyx", "w") as output:
            output.write(content)
        content = CommandUtils._gen_specs_py(specs)
        with open("pynodegl/specs.py", "w") as output:
            output.write(content)


class BuildExtCommand(build_ext):
    def run(self):
        CommandUtils.write_definitions_pyx()
        build_ext.run(self)


class BuildSrcCommmand(Command):
    user_options = []

    def initialize_options(self):
        pass

    def finalize_options(self):
        pass

    def run(self):
        from Cython.Compiler.Main import compile

        CommandUtils.write_definitions_pyx()
        compile("_pynodegl.pyx", output_file="_pynodegl.c")


setup(
    name="pynodegl",
    version=_LIB_CFG.version,
    packages=find_packages(include=["pynodegl"]),
    setup_requires=[
        "setuptools>=18.0",
        "cython>=0.29.6",
        "pyyaml",
    ],
    cmdclass={
        "build_ext": BuildExtCommand,
        "build_src": BuildSrcCommmand,
    },
    ext_modules=[
        Extension(
            "_pynodegl",
            sources=["_pynodegl.pyx"],
            include_dirs=_LIB_CFG.include_dirs,
            libraries=_LIB_CFG.libraries,
            library_dirs=_LIB_CFG.library_dirs,
        )
    ],
)
