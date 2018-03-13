import os
import subprocess

import pynodegl as ngl
from PyQt5 import QtGui, QtCore
from OpenGL import GL

from gl import get_gl_format
from com import query_inplace


class Exporter(QtCore.QObject):

    progressed = QtCore.pyqtSignal(int)

    def __init__(self, get_scene_func):
        super(Exporter, self).__init__()
        self._get_scene_func = get_scene_func

    def export(self, filename, w, h, extra_enc_args=None):
        fd_r, fd_w = os.pipe()

        cfg = self._get_scene_func(pipe=(fd_w, w, h))
        if not cfg:
            return None

        fps = cfg['framerate']
        duration = cfg['duration']
        samples = cfg['samples']

        cmd = ['ffmpeg', '-r', '%d/%d' % fps,
               '-nostats', '-nostdin',
               '-f', 'rawvideo',
               '-video_size', '%dx%d' % (w, h),
               '-pixel_format', 'rgba',
               '-i', 'pipe:%d' % fd_r]
        if extra_enc_args:
            cmd += extra_enc_args
        cmd += ['-y', filename]

        def close_unused_child_fd():
            os.close(fd_w)

        def close_unused_parent_fd():
            os.close(fd_r)

        reader = subprocess.Popen(cmd, preexec_fn=close_unused_child_fd, close_fds=False)
        close_unused_parent_fd()

        # GL context
        glctx = QtGui.QOpenGLContext()
        assert glctx.create() == True
        assert glctx.isValid() == True

        # Offscreen Surface
        surface = QtGui.QOffscreenSurface()
        surface.create()
        assert surface.isValid() == True

        glctx.makeCurrent(surface)

        # Framebuffer
        fbo_format = QtGui.QOpenGLFramebufferObjectFormat()
        fbo_format.setSamples(samples)
        fbo_format.setAttachment(QtGui.QOpenGLFramebufferObject.CombinedDepthStencil)
        fbo = QtGui.QOpenGLFramebufferObject(w, h, fbo_format)
        assert fbo.isValid() == True
        fbo.bind()

        # node.gl context
        ngl_viewer = ngl.Viewer()
        ngl_viewer.set_scene_from_string(cfg['scene'])
        ngl_viewer.configure(ngl.GLPLATFORM_AUTO, ngl.GLAPI_AUTO)
        GL.glViewport(0, 0, w, h)

        # Draw every frame
        nb_frame = int(duration * fps[0] / fps[1])
        for i in range(nb_frame):
            time = i * fps[1] / float(fps[0])
            # FIXME: due to the nature of Python threads, another widget can
            # make another GL context current once the GIL is released, thus we
            # need to make sure this rendering context is the current one
            # before each draw call.
            glctx.makeCurrent(surface)
            ngl_viewer.draw(time)
            self.progressed.emit(i*100 / nb_frame)
            glctx.swapBuffers(surface)
        self.progressed.emit(100)

        os.close(fd_w)
        fbo.release()
        glctx.doneCurrent()

        reader.wait()

        return cfg


def test_export():
    import sys

    def _get_scene(**cfg_overrides):
        cfg = {
            'pkg': 'pynodegl_utils.examples',
            'scene': ('misc', 'triangle'),
            'duration': 5,
        }
        cfg.update(cfg_overrides)

        ret = query_inplace(query='scene', **cfg)
        if 'error' in ret:
            print ret['error']
            return None
        return ret

    def print_progress(progress):
        sys.stdout.write('\r%d%%' % progress)
        if progress == 100:
            sys.stdout.write('\n')

    if len(sys.argv) != 2:
        print 'Usage: %s <outfile>' % sys.argv[0]
        sys.exit(0)

    QtGui.QSurfaceFormat.setDefaultFormat(get_gl_format())

    filename = sys.argv[1]
    app = QtGui.QGuiApplication(sys.argv)

    exporter = Exporter(_get_scene)
    exporter.progressed.connect(print_progress)
    exporter.export(filename, 320, 240)

if __name__ == '__main__':
    test_export()
