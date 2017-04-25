prefix=PREFIX
exec_prefix=${prefix}
includedir=${prefix}/include
libdir=${exec_prefix}/lib
datarootdir=${prefix}/share

Name: node.gl
Description: Node/Graph based OpenGL engine
Version: 0.0.0
Cflags: -I${includedir}
Libs: -L${libdir} -lnodegl DEP_LIBS
Libs.private: DEP_PRIVATE_LIBS
