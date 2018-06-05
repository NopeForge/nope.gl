import os
import subprocess

import pynodegl as ngl
from PyQt5 import QtGui, QtCore

from gl import get_gl_format
from com import query_inplace


class Exporter(QtCore.QThread):

    progressed = QtCore.pyqtSignal(int)
    failed = QtCore.pyqtSignal()

    def __init__(self, get_scene_func, filename, w, h, extra_enc_args=None):
        super(Exporter, self).__init__()
        self._get_scene_func = get_scene_func
        self._filename = filename
        self._width = w
        self._height = h
        self._extra_enc_args = extra_enc_args
        self._cancelled = False

    def run(self):
        fd_r, fd_w = os.pipe()

        cfg = self._get_scene_func(pipe=(fd_w, self._width, self._height))
        if not cfg:
            self.failed.emit()
            return

        fps = cfg['framerate']
        duration = cfg['duration']
        samples = cfg['samples']

        cmd = ['ffmpeg', '-r', '%d/%d' % fps,
               '-nostats', '-nostdin',
               '-f', 'rawvideo',
               '-video_size', '%dx%d' % (self._width, self._height),
               '-pixel_format', 'rgba',
               '-i', 'pipe:%d' % fd_r]
        if self._extra_enc_args:
            cmd += self._extra_enc_args
        cmd += ['-y', self._filename]

        def close_unused_child_fd():
            os.close(fd_w)

        def close_unused_parent_fd():
            os.close(fd_r)

        reader = subprocess.Popen(cmd, preexec_fn=close_unused_child_fd, close_fds=False)
        close_unused_parent_fd()

        # GL context
        glctx = QtGui.QOpenGLContext()
        assert glctx.create() is True
        assert glctx.isValid() is True

        # Offscreen Surface
        surface = QtGui.QOffscreenSurface()
        surface.create()
        assert surface.isValid() is True

        glctx.makeCurrent(surface)

        # Framebuffer
        fbo_format = QtGui.QOpenGLFramebufferObjectFormat()
        fbo_format.setSamples(samples)
        fbo_format.setAttachment(QtGui.QOpenGLFramebufferObject.CombinedDepthStencil)
        fbo = QtGui.QOpenGLFramebufferObject(self._width, self._height, fbo_format)
        assert fbo.isValid() is True
        fbo.bind()

        # node.gl context
        ngl_viewer = ngl.Viewer()
        ngl_viewer.configure(platform=ngl.GLPLATFORM_AUTO, api=ngl.GLAPI_AUTO)
        ngl_viewer.set_scene_from_string(cfg['scene'])
        ngl_viewer.set_viewport(0, 0, self._width, self._height)
        ngl_viewer.set_clearcolor(*cfg['clear_color'])

        # Draw every frame
        nb_frame = int(duration * fps[0] / fps[1])
        for i in range(nb_frame):
            if self._cancelled:
                break
            time = i * fps[1] / float(fps[0])
            ngl_viewer.draw(time)
            self.progressed.emit(i*100 / nb_frame)
            glctx.swapBuffers(surface)
        self.progressed.emit(100)

        os.close(fd_w)
        fbo.release()
        glctx.doneCurrent()

        reader.wait()

    def cancel(self):
        self._cancelled = True


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

    exporter = Exporter(_get_scene, filename, 320, 240)
    exporter.progressed.connect(print_progress)
    exporter.start()
    exporter.wait()


if __name__ == '__main__':
    test_export()
