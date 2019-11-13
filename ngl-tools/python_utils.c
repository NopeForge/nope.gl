/*
 * Copyright 2019 GoPro Inc.
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

static int check_error(void)
{
    if (!PyErr_Occurred())
        return 0;

    PyObject *ptype, *pvalue, *ptraceback;
    PyErr_Fetch(&ptype, &pvalue, &ptraceback);
    if (ptype)
        printf("type: %s\n", PyString_AsString(ptype));
    if (pvalue)
        printf("value: %s\n", PyString_AsString(pvalue));
    if (ptraceback)
        printf("traceback: %s\n", PyString_AsString(ptraceback));
    Py_XDECREF(ptype);
    Py_XDECREF(pvalue);
    Py_XDECREF(ptraceback);

    return -1;
}

struct ngl_node *python_get_scene(const char *modname, const char *func_name, double *duration)
{
    PyObject *pyscene = NULL;
    PyObject *mod = NULL, *utils = NULL;
    PyObject *ret_pydict = NULL;
    PyObject *scene_func = NULL, *cptr = NULL, *pyduration = NULL;
    struct ngl_node *scene = NULL;

    Py_Initialize();

    if (!(mod        = PyImport_ImportModule(modname))                 ||
        !(utils      = PyImport_ImportModule("pynodegl_utils.misc"))   ||
        !(scene_func = PyObject_GetAttrString(mod, func_name))         ||
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

    scene = PyLong_AsVoidPtr(cptr);
    ngl_node_ref(scene);

end:
    check_error();

    Py_XDECREF(mod);
    Py_XDECREF(utils);
    Py_XDECREF(scene_func);
    Py_XDECREF(ret_pydict);
    Py_XDECREF(cptr);

    Py_Finalize();
    return scene;
}
