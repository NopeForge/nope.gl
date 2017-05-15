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
#include <sys/time.h>

#include <GLFW/glfw3.h>
#include <Python.h>

#include <nodegl.h>

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

struct view_info {
    double x;
    double y;
    double width;
    double height;
};

#define ASPECT_RATIO (g_width / (double)g_height)

struct ngl_ctx *g_ctx;
int64_t g_clock_off = -1;
int64_t g_frame_ts;
struct view_info g_view_info;
int g_paused;
struct ngl_node *g_opacity_uniform;
int g_width = 1280;
int g_height = 800;
double g_duration;

static int64_t gettime()
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return 1000000 * (int64_t)tv.tv_sec + tv.tv_usec;
}

static int init(GLFWwindow *window, const char *modname, const char *funcname)
{
    g_ctx = ngl_create();
    ngl_set_glcontext(g_ctx, NULL, NULL, NULL, NGL_GLPLATFORM_AUTO, NGL_GLAPI_AUTO);
    glViewport(0, 0, g_width, g_height);

    struct ngl_node *scene = get_scene(modname, funcname, &g_duration);
    if (!scene)
        return -1;

    int ret = ngl_set_scene(g_ctx, scene);
    if (ret < 0)
        return ret;

    ngl_node_unrefp(&scene);
    return 0;
}

static double clipd(double v, double min, double max)
{
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

static void update_time(int64_t seek_at)
{
    if (seek_at >= 0) {
        g_clock_off = gettime() - seek_at;
        g_frame_ts = seek_at;
        return;
    }

    if (!g_paused) {
        const int64_t now = gettime();

        if (g_clock_off < 0)
            g_clock_off = now;

        g_frame_ts = now - g_clock_off;
    }
}

static void render(void)
{
    ngl_draw(g_ctx, g_frame_ts / 1000000.0);
}

static void reset(void)
{
    ngl_free(&g_ctx);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS) {
        switch(key) {
        case GLFW_KEY_ESCAPE:
        case GLFW_KEY_Q:
            glfwSetWindowShouldClose(window, GL_TRUE);
            break;
        case GLFW_KEY_SPACE:
            g_paused ^= 1;
            g_clock_off = gettime() - g_frame_ts;
            break;
        default:
            break;
        }
    }
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double pos;
        double xpos;
        double ypos;
        int64_t seek_at;

        glfwGetCursorPos(window, &xpos, &ypos);

        pos = clipd(xpos - g_view_info.x, 0.0, g_view_info.width);
        seek_at = 1000000 * g_duration * pos / g_view_info.width;

        update_time(seek_at);
    }
}

static void size_callback(GLFWwindow *window, int width, int height)
{
    g_view_info.width = width;
    g_view_info.height = width / ASPECT_RATIO;

    if (g_view_info.height > height) {
        g_view_info.height = height;
        g_view_info.width = height * ASPECT_RATIO;
    }

    g_view_info.x = (width - g_view_info.width) / 2.0;
    g_view_info.y = (height - g_view_info.height) / 2.0;

    glViewport(g_view_info.x, g_view_info.y, g_view_info.width, g_view_info.height);
}

int main(int argc, char *argv[])
{
    int ret;
    GLFWwindow *window;
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <module> <scene_func>\n", argv[0]);
        return -1;
    }

#ifdef __APPLE__
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    window = glfwCreateWindow(g_width, g_height, "ngl-python", NULL, NULL);
    if (window == NULL) {
        fprintf(stderr, "Failed to initialize GL context\n");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetWindowSizeCallback(window, size_callback);

    ret = init(window, argv[1], argv[2]);
    if (ret < 0)
        goto end;

    do {
        update_time(-1);
        render();
        glfwSwapBuffers(window);
        glfwPollEvents();
    } while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
             glfwWindowShouldClose(window) == 0);

end:
    reset();

    glfwDestroyWindow(window);
    glfwTerminate();

    return ret;
}
