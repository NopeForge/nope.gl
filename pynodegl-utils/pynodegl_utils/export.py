import os
import subprocess

import pynodegl as ngl
from PyQt5 import QtGui, QtCore

from com import query_inplace


class Exporter(QtCore.QThread):

    progressed = QtCore.pyqtSignal(int)
    failed = QtCore.pyqtSignal()
    finished = QtCore.pyqtSignal()

    def __init__(self, get_scene_func, filename, w, h, extra_enc_args=None):
        super(Exporter, self).__init__()
        self._get_scene_func = get_scene_func
        self._filename = filename
        self._width = w
        self._height = h
        self._extra_enc_args = extra_enc_args
        self._cancelled = False

    def run(self):
        filename, width, height = self._filename, self._width, self._height

        if filename.endswith('gif'):
            palette_filename = '/tmp/palette.png'
            pass1_args = ['-vf', 'palettegen']
            pass2_args = self._extra_enc_args + ['-i', palette_filename, '-lavfi', 'paletteuse']
            ok = self._export(palette_filename, width, height, pass1_args)
            if not ok:
                return
            ok = self._export(filename, width, height, pass2_args)
        else:
            ok = self._export(filename, width, height, self._extra_enc_args)
        if ok:
            self.finished.emit()

    def _export(self, filename, width, height, extra_enc_args=None):
        fd_r, fd_w = os.pipe()

        cfg = self._get_scene_func(pipe=(fd_w, width, height))
        if not cfg:
            self.failed.emit()
            return False

        fps = cfg['framerate']
        duration = cfg['duration']
        samples = cfg['samples']

        cmd = ['ffmpeg', '-r', '%d/%d' % fps,
               '-nostats', '-nostdin',
               '-f', 'rawvideo',
               '-video_size', '%dx%d' % (width, height),
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

        # node.gl context
        ngl_viewer = ngl.Viewer()
        ngl_viewer.configure(
            platform=ngl.GLPLATFORM_AUTO,
            api=ngl.GLAPI_AUTO,
            wrapped=0,
            offscreen=1,
            width=width,
            height=height,
            samples=samples,
        )
        ngl_viewer.set_scene_from_string(cfg['scene'])
        ngl_viewer.set_viewport(0, 0, width, height)
        ngl_viewer.set_clearcolor(*cfg['clear_color'])

        # Draw every frame
        nb_frame = int(duration * fps[0] / fps[1])
        for i in range(nb_frame):
            if self._cancelled:
                break
            time = i * fps[1] / float(fps[0])
            ngl_viewer.draw(time)
            self.progressed.emit(i*100 / nb_frame)
        self.progressed.emit(100)

        os.close(fd_w)
        reader.wait()
        return True

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
        sys.stdout.flush()
        if progress == 100:
            sys.stdout.write('\n')

    if len(sys.argv) != 2:
        print 'Usage: %s <outfile>' % sys.argv[0]
        sys.exit(0)

    filename = sys.argv[1]
    app = QtGui.QGuiApplication(sys.argv)

    exporter = Exporter(_get_scene, filename, 320, 240)
    exporter.progressed.connect(print_progress)
    exporter.start()
    exporter.wait()


if __name__ == '__main__':
    test_export()
