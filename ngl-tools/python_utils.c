/*
 * Copyright 2019-2022 GoPro Inc.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <Python.h>

#include <nopegl/nopegl.h>

#include "python_utils.h"

struct ngl_scene *python_get_scene(const char *modname, const char *func_name)
{
    PyObject *pyscene = NULL;
    PyObject *com = NULL, *load_script = NULL, *path = NULL;
    PyObject *mod = NULL;
    PyObject *scene_info = NULL;
    PyObject *scene_func = NULL;
    PyObject *scene_ptr = NULL;

    struct ngl_scene *scene = NULL;

    Py_Initialize();

    const size_t len = strlen(modname);
    if (len > 3 && !strcmp(modname + len - 3, ".py")) {
        if (!(com         = PyImport_ImportModule("pynopegl_utils.module")) ||
            !(load_script = PyObject_GetAttrString(com, "load_script"))  ||
            !(path        = PyUnicode_FromString(modname))               ||
            !(mod         = PyObject_CallFunctionObjArgs(load_script, path, NULL)))
            goto end;
    } else {
        if (!(mod = PyImport_ImportModule(modname)))
            goto end;
    }

    if (!(scene_func = PyObject_GetAttrString(mod, func_name))         ||
        !(scene_info = PyObject_CallFunctionObjArgs(scene_func, NULL)) ||
        !(pyscene    = PyObject_GetAttrString(scene_info, "scene"))    ||
        !(scene_ptr  = PyObject_GetAttrString(pyscene, "cptr"))) {
        goto end;
    }

    scene = ngl_scene_ref(PyLong_AsVoidPtr(scene_ptr));

end:
    if (PyErr_Occurred())
        PyErr_PrintEx(0);

    Py_XDECREF(com);
    Py_XDECREF(load_script);
    Py_XDECREF(path);
    Py_XDECREF(mod);
    Py_XDECREF(scene_func);
    Py_XDECREF(scene_info);
    Py_XDECREF(pyscene);
    Py_XDECREF(scene_ptr);

    Py_Finalize();
    return scene;
}
