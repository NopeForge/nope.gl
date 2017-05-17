/*
 * Copyright 2017 GoPro Inc.
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

#include <stdio.h>
#include <stdlib.h>

#include <Python.h>

#include <nodegl.h>

#include "common.h"
#include "player.h"

static PyObject *g_pyscene;

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

static struct ngl_node *get_scene(const char *modname, const char *func_name, double *duration)
{
    PyObject *mod = NULL, *utils = NULL;
    PyObject *scene_cfg_class = NULL, *scene_cfg = NULL;
    PyObject *scene_func = NULL, *cptr = NULL, *pyduration = NULL;
    struct ngl_node *scene = NULL;

    Py_Initialize();

    if (!(mod             = PyImport_ImportModule(modname))                     ||
        !(utils           = PyImport_ImportModule("pynodegl_utils.misc"))       ||
        !(scene_cfg_class = PyObject_GetAttrString(utils, "NGLSceneCfg"))       ||
        !(scene_cfg       = PyObject_CallObject(scene_cfg_class, NULL))         ||
        !(scene_func      = PyObject_GetAttrString(mod, func_name))             ||
        !(g_pyscene       = PyObject_CallFunction(scene_func, "O", scene_cfg))  ||
        !(cptr            = PyObject_GetAttrString(g_pyscene, "cptr"))          ||
        !(pyduration      = PyObject_GetAttrString(scene_cfg, "duration")))
        goto end;

    *duration = PyFloat_AsDouble(pyduration);
    if (PyErr_Occurred())
        goto end;

    scene = PyLong_AsVoidPtr(cptr);

end:
    check_error();

    Py_XDECREF(mod);
    Py_XDECREF(utils);
    Py_XDECREF(scene_cfg_class);
    Py_XDECREF(scene_cfg);
    Py_XDECREF(scene_func);
    Py_XDECREF(cptr);
    Py_XDECREF(pyduration);

    Py_Finalize();
    return scene;
}

int main(int argc, char *argv[])
{
    int ret;
    double duration;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <module> <scene_func>\n", argv[0]);
        return -1;
    }

    struct ngl_node *scene = get_scene(argv[1], argv[2], &duration);
    if (!scene)
        return -1;

    struct player p;
    ret = player_init(&p, "ngl-python", scene,
                      1280, 800, duration);
    if (ret < 0)
        goto end;
    ngl_node_unrefp(&scene);

    player_main_loop();

end:
    player_uninit();

    return ret;
}
