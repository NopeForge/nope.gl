import os
import os.path as op
import sys

from pynopegl_utils.viewer import run

os.environ["PATH"] = os.pathsep.join([op.join(sys._MEIPASS, "bin"), os.environ["PATH"]])

if __name__ == "__main__":
    run()
