from PyQt5 import QtGui

def get_gl_format(renderable='gl', version=None):
    gl_format = QtGui.QSurfaceFormat()

    if renderable == 'gl':
        gl_format.setRenderableType(QtGui.QSurfaceFormat.OpenGL)
        major_version, minor_version = (3, 3) if version is None else version
        gl_format.setVersion(major_version, minor_version)
        gl_format.setProfile(QtGui.QSurfaceFormat.CoreProfile)
    elif renderable == 'gles':
        gl_format.setRenderableType(QtGui.QSurfaceFormat.OpenGLES)
        major_version, minor_version = (2, 0) if version is None else version
        gl_format.setVersion(major_version, minor_version)
    else:
        raise Exception('Unknown rendering backend' % renderable)

    gl_format.setDepthBufferSize(24)
    gl_format.setStencilBufferSize(8)
    gl_format.setAlphaBufferSize(8)
    return gl_format
