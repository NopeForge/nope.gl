from libc.stdlib cimport calloc
from libc.string cimport memset

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
    ngl_node *ngl_node_deserialize(const char *s)

    int ngl_anim_evaluate(ngl_node *anim, void *dst, double t)

    cdef int NGL_GLPLATFORM_AUTO
    cdef int NGL_GLPLATFORM_GLX
    cdef int NGL_GLPLATFORM_EGL
    cdef int NGL_GLPLATFORM_NSGL
    cdef int NGL_GLPLATFORM_EAGL

    cdef int NGL_GLAPI_AUTO
    cdef int NGL_GLAPI_OPENGL
    cdef int NGL_GLAPI_OPENGLES

    cdef struct ngl_ctx

    cdef struct ngl_config:
        void *display
        void *window
        void *handle
        int  platform
        int  api
        int  wrapped
        int  swap_interval
        int  offscreen
        int  width
        int  height
        int  samples

    ngl_ctx *ngl_create()
    int ngl_configure(ngl_ctx *s, ngl_config *config)
    int ngl_set_scene(ngl_ctx *s, ngl_node *scene)
    int ngl_set_viewport(ngl_ctx *s, int x, int y, int width, int height)
    int ngl_set_clearcolor(ngl_ctx *s, double r, double g, double b, double a)
    int ngl_draw(ngl_ctx *s, double t) nogil
    char *ngl_dot(ngl_ctx *s, double t) nogil
    void ngl_free(ngl_ctx **ss)

GLPLATFORM_AUTO = NGL_GLPLATFORM_AUTO
GLPLATFORM_GLX  = NGL_GLPLATFORM_GLX
GLPLATFORM_EGL  = NGL_GLPLATFORM_EGL
GLPLATFORM_NSGL = NGL_GLPLATFORM_NSGL
GLPLATFORM_EAGL = NGL_GLPLATFORM_EAGL

GLAPI_AUTO      = NGL_GLAPI_AUTO
GLAPI_OPENGL    = NGL_GLAPI_OPENGL
GLAPI_OPENGLES  = NGL_GLAPI_OPENGLES

LOG_VERBOSE = NGL_LOG_VERBOSE
LOG_DEBUG   = NGL_LOG_DEBUG
LOG_INFO    = NGL_LOG_INFO
LOG_WARNING = NGL_LOG_WARNING
LOG_ERROR   = NGL_LOG_ERROR

cdef _ret_pystr(char *s):
    try:
        pystr = <bytes>s
    finally:
        free(s)
    return pystr

include "nodes_def.pyx"

def log_set_min_level(int level):
    ngl_log_set_min_level(level)

cdef class Viewer:
    cdef ngl_ctx *ctx

    def __cinit__(self):
        self.ctx = ngl_create()
        if self.ctx is NULL:
            raise MemoryError()

    def configure(self, **kwargs):
        cdef ngl_config config
        memset(&config, 0, sizeof(config));
        config.platform = kwargs.get('platform', GLPLATFORM_AUTO)
        config.api = kwargs.get('api', GLAPI_AUTO)
        config.wrapped = kwargs.get('wrapped', 0)
        config.swap_interval = kwargs.get('swap_interval', -1)
        config.offscreen = kwargs.get('offscreen', 0)
        config.width = kwargs.get('width', 0)
        config.height = kwargs.get('height', 0)
        config.samples = kwargs.get('samples', 0)
        return ngl_configure(self.ctx, &config)

    def set_scene(self, _Node scene):
        return ngl_set_scene(self.ctx, scene.ctx)

    def set_viewport(self, int x, int y, int width, int height):
        return ngl_set_viewport(self.ctx, x, y, width, height)

    def set_clearcolor(self, double r, double g, double b, double a):
        return ngl_set_clearcolor(self.ctx, r, g, b, a)

    def set_scene_from_string(self, s):
        cdef ngl_node *scene = ngl_node_deserialize(s);
        ret = ngl_set_scene(self.ctx, scene)
        ngl_node_unrefp(&scene)
        return ret

    def draw(self, double t):
        with nogil:
            ngl_draw(self.ctx, t)

    def dot(self, double t):
        cdef char *s;
        with nogil:
            s = ngl_dot(self.ctx, t)
        return _ret_pystr(s) if s else None

    def __dealloc__(self):
        ngl_free(&self.ctx)
