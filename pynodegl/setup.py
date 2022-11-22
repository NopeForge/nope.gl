#
# Copyright 2016-2022 GoPro Inc.
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

import json
import textwrap

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


class _WrapperGenerator:
    """
    Helper to generate Python class declarations of all the nodes.
    """

    _ARG_TYPES = {"bool", "f32", "f64", "flags", "i32", "select", "str", "u32", "data", "node"}
    _ARGS_TYPES = {"ivec2", "ivec3", "ivec4", "mat4", "uvec2", "uvec3", "uvec4", "vec2", "vec3", "vec4"}

    _TYPING_MAP = dict(
        bool="bool",
        data="array.array",
        f32="float",
        f64="float",
        f64_list="Sequence[float]",
        flags="str",
        i32="int",
        ivec2="Tuple[int, int]",
        ivec3="Tuple[int, int, int]",
        ivec4="Tuple[int, int, int, int]",
        mat4="Tuple[" + ", ".join(["float"] * 16) + "]",
        node="Node",
        node_dict="Mapping[str, Node]",
        node_list="Sequence[Node]",
        rational="Tuple[int, int]",
        select="str",
        str="str",
        u32="int",
        uvec2="Tuple[int, int]",
        uvec3="Tuple[int, int, int]",
        uvec4="Tuple[int, int, int, int]",
        vec2="Tuple[float, float]",
        vec3="Tuple[float, float, float]",
        vec4="Tuple[float, float, float, float]",
    )

    _TYPING_SINGLE_MAP = dict(
        ivec2="int",
        ivec3="int",
        ivec4="int",
        mat4="float",
        uvec2="int",
        uvec3="int",
        uvec4="int",
        vec2="float",
        vec3="float",
        vec4="float",
    )

    def __init__(self, specs):
        self._specs = specs

    def generate(self):
        classes = self._declare_classes()
        livectl_info = self._livectl_info()
        return classes + "\n\n" + livectl_info

    @classmethod
    def _get_setter_code(cls, param_name, param_type, has_kwargs=False):
        if param_type in cls._ARG_TYPES:
            return f'self._arg_setter(self._param_set_{param_type}, "{param_name}", {param_name})'
        if param_type in cls._ARGS_TYPES:
            return f'self._args_setter(self._param_set_{param_type}, "{param_name}", *{param_name})'
        if param_type == "node_list":
            return f'self._add_nodes("{param_name}", *{param_name})'
        if param_type == "f64_list":
            return f'self._add_f64s("{param_name}", *{param_name})'
        if param_type == "node_dict":
            if has_kwargs:
                return f'self._update_dict("{param_name}", {param_name}, **kwargs)'
            return f'self._update_dict("{param_name}", {param_name})'
        if param_type == "rational":
            return f'self._set_rational("{param_name}", {param_name})'
        assert False

    @classmethod
    def _get_setter_prototype(cls, param_name, param_type, param_flags):
        type_ = cls._TYPING_SINGLE_MAP.get(param_type, cls._TYPING_MAP[param_type])
        if "N" in param_flags:
            type_ = f"Union[{type_}, Node]"
        if param_type in cls._ARG_TYPES:
            return f"set_{param_name}(self, {param_name}: {type_})"
        if param_type in cls._ARGS_TYPES:
            return f"set_{param_name}(self, *{param_name}: {type_})"
        if param_type == "node_list":
            return f"add_{param_name}(self, *{param_name}: Node)"
        if param_type == "f64_list":
            return f"add_{param_name}(self, *{param_name}: float)"
        if param_type == "node_dict":
            return f"update_{param_name}(self, {param_name}: Optional[{type_}] = None, **kwargs: Optional[Node])"
        if param_type == "rational":
            return f"set_{param_name}(self, {param_name}: Tuple[int, int])"
        assert False

    @classmethod
    def _get_class_setters(cls, params):
        setters = []
        for param_name, param_type, param_flags in params:
            prototype = cls._get_setter_prototype(param_name, param_type, param_flags)
            setter_code = cls._get_setter_code(param_name, param_type, has_kwargs=True)
            setters.append(
                textwrap.dedent(
                    f"""
                    def {prototype} -> int:
                        return {setter_code}
                    """
                )
            )
        return "".join(setters)

    @classmethod
    def _get_class_evaluate(cls, class_name):
        animated_nodes = dict(
            AnimatedFloat="f32",
            AnimatedVec2="vec2",
            AnimatedVec3="vec3",
            AnimatedVec4="vec4",
            AnimatedQuat="vec4",
            VelocityFloat="f32",
            VelocityVec2="vec2",
            VelocityVec3="vec3",
            VelocityVec4="vec4",
        )

        eval_type = animated_nodes.get(class_name)
        if not eval_type:
            return ""
        ret_type = cls._TYPING_MAP[eval_type]
        return textwrap.dedent(
            f"""
            def evaluate(self, t: float) -> {ret_type}:
                return self._eval_{eval_type}(t)
            """
        )

    @classmethod
    def _get_class_init(cls, parent_params, params, inherited):

        # Generate the code responsible for honoring all direct parameters (if
        # they are not None)
        init_code = ""
        for param_name, param_type, param_flags in params:
            setter_code = cls._get_setter_code(param_name, param_type)

            if param_type in cls._ARGS_TYPES and "N" in param_flags:
                # Special case for parameter that can be either a multi-value
                # argument, or a single node. We need to bridge here because we
                # don't know yet if we need to unpack the args contrary to
                # within the setter context
                init_code += textwrap.dedent(
                    f"""\
                    if {param_name} is not None:
                        if isinstance({param_name}, Node):
                            self._param_set_node("{param_name}", {param_name})
                        else:
                            {setter_code}
                    """
                )
            else:
                init_code += textwrap.dedent(
                    f"""\
                    if {param_name} is not None:
                        {setter_code}
                    """
                )

        # Build the __init__() (or _init_params()) by aggregating all the
        # parameters from the current node and the parents. This aggregation
        # allow us to get rid of the use of obscure **kwargs.
        kwargs = ["self"]
        for param_name, param_type, param_flags in params + parent_params:
            param_type = cls._TYPING_MAP[param_type]
            if "N" in param_flags:
                param_type = f"Union[{param_type}, Node]"
            kwargs.append(f"{param_name}: Optional[{param_type}] = None")

        # The leaf is the only node supposed to overload the __init__ method
        # because it must call _pynodegl._Node.__init__ with the context before
        # any parameter is set. That is also why only the leaf node will have a
        # ctx parameter.
        if not inherited:
            kwargs.append("ctx: int = 0")
        method_name = "_init_params" if inherited else "__init__"

        indent = " " * 4
        init = f"\ndef {method_name}(\n"
        init += textwrap.indent(",\n".join(kwargs), indent)
        init += "\n):\n"
        if not inherited:
            init += textwrap.indent("super().__init__(ctx)\n", indent)
        init += textwrap.indent(init_code, indent)

        # If the parents have parameters, they are forwarded at the end, after
        # honoring the current ones.
        if parent_params:
            kwargs_forward = ", ".join(f"{pname}={pname}" for pname, _, _ in parent_params)
            init += textwrap.indent(f"super()._init_params({kwargs_forward})\n", indent)

        return init

    @staticmethod
    def _get_type_id(class_name):
        return "_ngl.NODE_" + class_name.upper()

    def _declare_class(self, class_name, params):
        specs = self._specs

        # The inheritance of node classes has 4 to 5 layers, from bottom to
        # top:
        # - _pynodegl._Node: the native Cython node
        # - pynodegl.Node (from node.py): contains Python shared code
        # - pynodegl._CommonNode: generated according to the parameters of
        #   "_Node" in the nodes.specs
        # - pynodegl._***: generated and optional, these are intermediate
        #   inherited nodes (for example _AnimatedBuffer, which contains all
        #   the parameters shared by AnimatedBuffer* nodes)
        # - pynodegl.***: leaf node, accessed by the users; may contain
        #   parameters only if they are not inheriting from an intermediate
        #   optional node
        if class_name == "_Node":
            parent_class_name = "Node"  # inherit from native Cython node
            parent_params = []
            class_name = "_CommonNode"
        elif isinstance(params, str):  # inherit from an intermediate node
            parent_class_name = params
            params = []
            parent_params = specs[parent_class_name] + specs["_Node"]
        else:
            parent_class_name = "_CommonNode"
            parent_params = specs["_Node"]

        if params is None:
            params = []
        if parent_params is None:
            parent_params = []

        # Only leaves have a type ID
        inherited = class_name[0] == "_"
        type_id = None if inherited else self._get_type_id(class_name)

        # Generate class code: init, setters and potential evaluate methods
        init_code = self._get_class_init(parent_params, params, inherited)
        setters_code = self._get_class_setters(params)
        evaluate_code = self._get_class_evaluate(class_name)

        class_code = f"class {class_name}({parent_class_name}):\n"
        indent = " " * 4
        if type_id:
            class_code += textwrap.indent(f"type_id = {type_id}\n", indent)
        class_code += textwrap.indent(init_code, indent)
        class_code += textwrap.indent(setters_code, indent)
        class_code += textwrap.indent(evaluate_code, indent)
        return class_code

    def _declare_classes(self):
        return "\n\n".join(self._declare_class(class_name, params) for class_name, params in self._specs.items())

    def _livectl_info(self):
        info_entries = []
        for class_name, params in self._specs.items():
            if class_name[0] == "_" or not params or isinstance(params, str):
                continue

            dparams = {k: (dtype, flags) for k, dtype, flags in params}
            if "live_id" not in dparams:
                continue

            # Identify the first parameter with a "live" flag as the reference value
            data_type = next(dtype for dtype, flags in dparams.values() if "L" in flags)
            # Expose enough information to the Cython such that it is able
            # to construct and expose a usable dict of live controls to the
            # user
            type_id = self._get_type_id(class_name)
            info_entries.append(f'{type_id}: ({class_name}, "{data_type}"),')

        livectl_info = textwrap.indent("\n".join(info_entries), " " * 4)
        return "_ngl.LIVECTL_INFO.update({\n" + livectl_info + "\n})\n"


class CommandUtils:
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

        specs_file = op.join(_LIB_CFG.data_root_dir, "nodegl", "nodes.specs")
        with open(specs_file) as f:
            specs = json.load(f)
        content = CommandUtils._gen_definitions_pyx(specs)
        with open("nodes_def.pyx", "w") as output:
            output.write(content)
        with open("init_header.py") as header:
            content = header.read() + "\n\n"
        content += _WrapperGenerator(specs).generate()
        with open("pynodegl/__init__.py", "w") as output:
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
