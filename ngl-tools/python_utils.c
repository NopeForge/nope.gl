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

#include <nodegl.h>

#include "python_utils.h"

struct ngl_node *python_get_scene(const char *modname, const char *func_name, double *duration, int *aspect)
{
    PyObject *pyscene = NULL;
    PyObject *com = NULL, *load_script = NULL, *path = NULL;
    PyObject *mod = NULL;
    PyObject *ret_pydict = NULL;
    PyObject *scene_func = NULL, *cptr = NULL, *pyduration = NULL;
    struct ngl_node *scene = NULL;

    Py_Initialize();

    const size_t len = strlen(modname);
    if (len > 3 && !strcmp(modname + len - 3, ".py")) {
        if (!(com         = PyImport_ImportModule("pynodegl_utils.module")) ||
            !(load_script = PyObject_GetAttrString(com, "load_script"))  ||
            !(path        = PyUnicode_FromString(modname))               ||
            !(mod         = PyObject_CallFunctionObjArgs(load_script, path, NULL)))
            goto end;
    } else {
        if (!(mod = PyImport_ImportModule(modname)))
            goto end;
    }

    if (!(scene_func = PyObject_GetAttrString(mod, func_name))         ||
        !(ret_pydict = PyObject_CallFunctionObjArgs(scene_func, NULL)) ||
        !(pyscene    = PyDict_GetItemString(ret_pydict, "scene"))      ||
        !(cptr       = PyObject_GetAttrString(pyscene, "cptr"))        ||
        !(pyduration = PyDict_GetItemString(ret_pydict, "duration"))) {
        goto end;
    }

    if (duration) {
        *duration = PyFloat_AsDouble(pyduration);
        if (PyErr_Occurred())
            goto end;
    }

    if (aspect) {
        PyObject *pyaspect = PyDict_GetItemString(ret_pydict, "aspect_ratio");
        if (!pyaspect)
            goto end;
        PyObject *pyaspect0, *pyaspect1;
        if (!(pyaspect0 = PyTuple_GetItem(pyaspect, 0)) ||
            !(pyaspect1 = PyTuple_GetItem(pyaspect, 1)))
            goto end;
        aspect[0] = PyLong_AsLong(pyaspect0);
        if (PyErr_Occurred())
            goto end;
        aspect[1] = PyLong_AsLong(pyaspect1);
        if (PyErr_Occurred())
            goto end;
    }

    scene = PyLong_AsVoidPtr(cptr);
    ngl_node_ref(scene);

end:
    if (PyErr_Occurred())
        PyErr_PrintEx(0);

    Py_XDECREF(com);
    Py_XDECREF(load_script);
    Py_XDECREF(path);
    Py_XDECREF(mod);
    Py_XDECREF(scene_func);
    Py_XDECREF(ret_pydict);
    Py_XDECREF(cptr);

    Py_Finalize();
    return scene;
}
