from libc.stdlib cimport calloc

cdef extern from "nodegl.h":
    cdef int NGL_LOG_VERBOSE
    cdef int NGL_LOG_DEBUG
    cdef int NGL_LOG_INFO
    cdef int NGL_LOG_WARNING
    cdef int NGL_LOG_ERROR

    void ngl_log_set_min_level(int level)

    cdef struct ngl_node

    ngl_node *ngl_node_create(int type, ...)
    ngl_node *ngl_node_ref(ngl_node *node)
    void ngl_node_unrefp(ngl_node **nodep)

    int ngl_node_param_add(ngl_node *node, const char *key,
                           int nb_elems, void *elems)
    int ngl_node_param_set(ngl_node *node, const char *key, ...)
    char *ngl_node_dot(const ngl_node *node)
    char *ngl_node_serialize(const ngl_node *node)

    void ngl_anim_evaluate(ngl_node *anim, float *dst, double t)

    cdef int NGL_GLPLATFORM_AUTO
    cdef int NGL_GLPLATFORM_GLX
    cdef int NGL_GLPLATFORM_EGL
    cdef int NGL_GLPLATFORM_CGL
    cdef int NGL_GLPLATFORM_EAGL

    cdef int NGL_GLAPI_AUTO
    cdef int NGL_GLAPI_OPENGL3
    cdef int NGL_GLAPI_OPENGLES2

    cdef struct ngl_ctx

    ngl_ctx *ngl_create()
    int ngl_set_glcontext(ngl_ctx *s, void *display, void *window, void *handle, int platform, int api)
    int ngl_set_glstates(ngl_ctx *s, int nb_glstates,  ngl_node **glstates);
    int ngl_set_scene(ngl_ctx *s, ngl_node *scene)
    int ngl_draw(ngl_ctx *s, double t) nogil
    void ngl_free(ngl_ctx **ss)

GLPLATFORM_AUTO = NGL_GLPLATFORM_AUTO
GLPLATFORM_GLX  = NGL_GLPLATFORM_GLX
GLPLATFORM_EGL  = NGL_GLPLATFORM_EGL
GLPLATFORM_CGL  = NGL_GLPLATFORM_CGL
GLPLATFORM_EAGL = NGL_GLPLATFORM_EAGL

GLAPI_AUTO      = NGL_GLAPI_AUTO
GLAPI_OPENGL3   = NGL_GLAPI_OPENGL3
GLAPI_OPENGLES2 = NGL_GLAPI_OPENGLES2

LOG_VERBOSE = NGL_LOG_VERBOSE
LOG_DEBUG   = NGL_LOG_DEBUG
LOG_INFO    = NGL_LOG_INFO
LOG_WARNING = NGL_LOG_WARNING
LOG_ERROR   = NGL_LOG_ERROR

include "nodes_def.pyx"

def log_set_min_level(int level):
    ngl_log_set_min_level(level)

cdef class Viewer:
    cdef ngl_ctx *ctx

    def __cinit__(self):
        self.ctx = ngl_create()
        if self.ctx is NULL:
            raise MemoryError()

    def configure(self, int platform, int api):
        return ngl_set_glcontext(self.ctx, NULL, NULL, NULL, platform, api);

    def set_glstates(self, *glstates):
        if not glstates:
            return ngl_set_glstates(self.ctx, 0, NULL)

        if hasattr(glstates[0], '__iter__'):
            raise Exception("set_glstates() takes elements as "
                            "positional arguments, not list")
        glstates_c = <ngl_node **>calloc(len(glstates), sizeof(ngl_node *))
        if glstates_c is NULL:
            raise MemoryError()
        for i, item in enumerate(glstates):
            glstates_c[i] = (<_Node>item).ctx

        ret = ngl_set_glstates(self.ctx, len(glstates), glstates_c)
        free(glstates_c)
        return ret

    def set_scene(self, _Node scene):
        return ngl_set_scene(self.ctx, scene.ctx)

    def draw(self, double t):
        with nogil:
            ngl_draw(self.ctx, t)

    def __dealloc__(self):
        ngl_free(&self.ctx)
