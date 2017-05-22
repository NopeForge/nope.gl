from PyQt5 import QtGui

def get_gl_format():
    gl_format = QtGui.QSurfaceFormat()
    gl_format.setVersion(3, 3)
    gl_format.setProfile(QtGui.QSurfaceFormat.CoreProfile)
    gl_format.setDepthBufferSize(24)
    gl_format.setStencilBufferSize(8)
    gl_format.setAlphaBufferSize(8)
    return gl_format
