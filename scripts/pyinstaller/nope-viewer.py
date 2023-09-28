import os
import os.path as op
import platform
import sys

from pynopegl_utils.viewer import run

os.environ["PATH"] = os.pathsep.join([op.join(sys._MEIPASS, "bin"), os.environ["PATH"]])

if __name__ == "__main__":
    if (
        platform.system() == "Windows"
        and len(sys.argv) > 3
        and sys.argv[1] == "-m"
        and sys.argv[2] == "pynopegl_utils.viewer.ffmpeg_win32"
    ):
        from pynopegl_utils.viewer.ffmpeg_win32 import run as run_ffmpeg

        run_ffmpeg(sys.argv[3:])
    else:
        run()
