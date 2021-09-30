#
# Copyright 2021 GoPro Inc.
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

from .specs import SPECS
import _pynodegl as _ngl

from _pynodegl import (
    log_set_min_level,
    easing_evaluate,
    easing_derivate,
    easing_solve,
    probe_backends,
    get_backends,
    Context,
)


def _create_arg_setter(cython_setter, param_name):
    def arg_setter(self, arg):
        if isinstance(arg, _Node):
            return self._param_set_node(param_name, arg)
        return cython_setter(self, param_name, arg)
    return arg_setter


def _create_args_setter(cython_setter, param_name):
    def args_setter(self, *args):
        if args and isinstance(args[0], _Node):
            return self._param_set_node(param_name, args[0])
        return cython_setter(self, param_name, args)
    return args_setter


def _create_add_nodes(param_name):
    def add_nodes(self, *nodes):
        if hasattr(nodes[0], '__iter__'):
            raise Exception(f"add_{param_name}() takes elements as positional arguments, not list")
        return self._param_add_nodes(param_name, len(nodes), nodes)
    return add_nodes


def _create_add_f64s(param_name):
    def add_f64s(self, *f64s):
        if hasattr(f64s[0], '__iter__'):
            raise Exception(f"add_{param_name}() takes elements as positional arguments, not list")
        return self._param_add_f64s(param_name, len(f64s), f64s)
    return add_f64s


def _create_update_dict(param_name):
    def update_dict(self, arg=None, **kwargs):
        data_dict = {}
        if arg is not None:
            if not isinstance(arg, dict):
                raise TypeError(f'{param_name} must be of type dict')
            data_dict.update(arg)
        data_dict.update(**kwargs)
        for key, val in data_dict.items():
            if not isinstance(key, str) or (val is not None and not isinstance(val, _Node)):
                raise TypeError(f'update_{param_name}() takes a dictionary of <string, node>')
            ret = self._param_set_dict(param_name, key, val)
            if ret < 0:
                return ret
        return 0
    return update_dict


def _create_set_data(param_name):
    def set_data(self, arg):
        return self._param_set_data(param_name, arg)
    return set_data


def _create_set_rational(param_name):
    def set_rational(self, ratio):
        return self._param_set_rational(param_name, ratio[0], ratio[1])
    return set_rational


def _create_set_node(param_name):
    def set_node(self, node):
        return self._param_set_node(param_name, node)
    return set_node


def _init_param(self, param_name, param_type, value):
    if ('vec' in param_type or param_type == 'mat4') and not isinstance(value, _Node):
        getattr(self, f'set_{param_name}')(*value)
    elif param_type == 'dict':
        getattr(self, f'update_{param_name}')(value)
    elif param_type.endswith('_list'):
        getattr(self, f'add_{param_name}')(*value)
    else:
        getattr(self, f'set_{param_name}')(value)


def _set_class_init(cls, base_class):
    def cls_init(self, *args, **kwargs):
        base_class.__init__(self)

        # Consume args and kwargs from the leaf (user node) up to the root
        # (ngl._Node)
        for cls in self.__class__.mro():
            params = getattr(cls, '_params', None)
            if not params:
                continue

            # consume args
            args = list(args)
            for param_name, param_type in params[:len(args)]:
                value = args.pop(0)
                if value is None:
                    continue
                _init_param(self, param_name, param_type, value)

            # consume kwargs
            for param_name, param_type in params[len(args):]:
                value = kwargs.pop(param_name, None)
                if value is None:
                    continue
                _init_param(self, param_name, param_type, value)

        if args or kwargs:
            raise TypeError(f"remaining arguments not consumed: {args=} {kwargs=}")

    cls.__init__ = cls_init


def _set_class_setters(cls):
    if not cls._params:
        return

    cython_arg_setters = dict(
        bool=cls._param_set_bool,
        f32=cls._param_set_f32,
        f64=cls._param_set_f64,
        flags=cls._param_set_flags,
        i32=cls._param_set_i32,
        select=cls._param_set_select,
        str=cls._param_set_str,
        u32=cls._param_set_u32,
    )
    cython_args_setters = dict(
        ivec2=cls._param_set_ivec2,
        ivec3=cls._param_set_ivec3,
        ivec4=cls._param_set_ivec4,
        mat4=cls._param_set_mat4,
        uvec2=cls._param_set_uvec2,
        uvec3=cls._param_set_uvec3,
        uvec4=cls._param_set_uvec4,
        vec2=cls._param_set_vec2,
        vec3=cls._param_set_vec3,
        vec4=cls._param_set_vec4,
    )

    for param_name, param_type in cls._params:
        if param_type in cython_arg_setters:
            cython_setter = cython_arg_setters.get(param_type)
            setattr(cls, f'set_{param_name}', _create_arg_setter(cython_setter, param_name))
        elif param_type in cython_args_setters:
            cython_setter = cython_args_setters.get(param_type)
            setattr(cls, f'set_{param_name}', _create_args_setter(cython_setter, param_name))
        elif param_type == 'node_list':
            setattr(cls, f'add_{param_name}', _create_add_nodes(param_name))
        elif param_type == 'f64_list':
            setattr(cls, f'add_{param_name}', _create_add_f64s(param_name))
        elif param_type == 'node_dict':
            setattr(cls, f'update_{param_name}', _create_update_dict(param_name))
        elif param_type == 'data':
            setattr(cls, f'set_{param_name}', _create_set_data(param_name))
        elif param_type == 'rational':
            setattr(cls, f'set_{param_name}', _create_set_rational(param_name))
        elif param_type == 'node':
            setattr(cls, f'set_{param_name}', _create_set_node(param_name))
        else:
            assert False


def _set_class_eval_method(cls, class_name):
    animated_nodes = dict(
        AnimatedFloat=cls._eval_f32,
        AnimatedVec2=cls._eval_vec2,
        AnimatedVec3=cls._eval_vec3,
        AnimatedVec4=cls._eval_vec4,
        AnimatedQuat=cls._eval_vec4,
        VelocityFloat=cls._eval_f32,
        VelocityVec2=cls._eval_vec2,
        VelocityVec3=cls._eval_vec3,
        VelocityVec4=cls._eval_vec4,
    )

    eval_method = animated_nodes.get(class_name)
    if eval_method:
        cls.evaluate = eval_method


def _declare_class(class_name, params):
    attr = dict(_params=params)
    if class_name[0] == '_':
        base_class = _ngl._Node if class_name == '_Node' else _Node
    else:
        # Finite user node with an ID
        attr.update(type_id=getattr(_ngl, f'NODE_{class_name.upper()}'))

        # Handle node params inheritance vs generic _Node
        if isinstance(params, str):
            base_class = globals()[params]
            attr['_params'] = None
        else:
            base_class = _Node

    # Create a new class for this type of node
    cls = type(class_name, (base_class,), attr)
    globals()[class_name] = cls

    # Set various node methods dynamically
    _set_class_setters(cls)
    if class_name[0] != '_':
        _set_class_init(cls, base_class)
        _set_class_eval_method(cls, class_name)


def _declare_classes():
    for class_name, params in SPECS.items():
        _declare_class(class_name, params)


def _declare_constants():
    for elem in dir(_ngl):
        if elem.startswith(('PLATFORM_', 'BACKEND_', 'CAP_', 'LOG_')):
            globals()[elem] = getattr(_ngl, elem)


_declare_classes()
_declare_constants()
