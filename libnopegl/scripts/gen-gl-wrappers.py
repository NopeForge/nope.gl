#!/usr/bin/env python3
#
# Copyright 2017-2022 GoPro Inc.
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

import sys
import textwrap
from xml.etree import ElementTree as ET

cmds_optional = [
    # Texture
    "glTexStorage2D",
    "glTexStorage3D",
    "glBindImageTexture",
    # Barrier
    "glMemoryBarrier",
    #  Buffers
    "glBindBufferBase",
    "glBindBufferRange",
    "glBufferStorage",
    # Compute shaders
    "glDispatchCompute",
    # Shaders
    "glGetProgramResourceLocation",
    "glGetProgramResourceIndex",
    "glGetProgramResourceiv",
    "glGetProgramInterfaceiv",
    "glGetProgramResourceName",
    # Internal format
    "glGetInternalformativ",
    # Buffer Storage EXT
    "glBufferStorageEXT",
    # Debug
    "glDebugMessageCallback",
    # Query
    "glBeginQuery",
    "glEndQuery",
    "glGenQueries",
    "glDeleteQueries",
    "glQueryCounter",
    "glGetQueryObjectui64v",
    # Debug
    "glDebugMessageCallback",
    # Query EXT
    "glBeginQueryEXT",
    "glEndQueryEXT",
    "glGenQueriesEXT",
    "glDeleteQueriesEXT",
    "glQueryCounterEXT",
    "glGetQueryObjectui64vEXT",
    # EGL OES image
    "glEGLImageTargetTexture2DOES",
    # Invalidate subdat
    "glInvalidateFramebuffer",
]

cmds = [
    # Enable
    "glEnable",
    "glDisable",
    # Error
    "glGetError",
    # Get
    "glGetBooleanv",
    "glGetIntegeri_v",
    "glGetIntegerv",
    "glGetString",
    "glGetStringi",
    # Viewport
    "glViewport",
    "glScissor",
    # Color
    "glColorMask",
    # Depth
    "glDepthFunc",
    "glDepthMask",
    # Clear
    "glClear",
    "glClearColor",
    # Blending
    "glBlendColor",
    "glBlendEquation",
    "glBlendEquationSeparate",
    "glBlendFunc",
    "glBlendFuncSeparate",
    # Draw
    "glDrawArrays",
    "glDrawElements",
    # Instancing
    "glDrawArraysInstanced",
    "glDrawElementsInstanced",
    "glVertexAttribDivisor",
    # Texture
    "glActiveTexture",
    "glBindTexture",
    "glDeleteTextures",
    "glGenTextures",
    "glGenerateMipmap",
    "glPixelStorei",
    "glTexImage2D",
    "glTexParameteri",
    "glTexSubImage2D",
    "glTexImage3D",
    "glTexSubImage3D",
    # Framebuffer
    "glCheckFramebufferStatus",
    "glBindFramebuffer",
    "glDeleteFramebuffers",
    "glFramebufferRenderbuffer",
    "glFramebufferTexture2D",
    "glGenFramebuffers",
    "glReadPixels",
    "glBlitFramebuffer",
    "glGetFramebufferAttachmentParameteriv",
    "glFramebufferTextureLayer",
    # Renderbuffer
    "glRenderbufferStorageMultisample",
    # Buffer
    "glBindBuffer",
    "glBufferData",
    "glBufferSubData",
    "glDeleteBuffers",
    "glGenBuffers",
    # Render buffer
    "glBindRenderbuffer",
    "glDeleteRenderbuffers",
    "glGenRenderbuffers",
    "glGetRenderbufferParameteriv",
    "glRenderbufferStorage",
    # Shader
    "glCreateProgram",
    "glCreateShader",
    "glAttachShader",
    "glCompileShader",
    "glDeleteProgram",
    "glDeleteShader",
    "glDetachShader",
    "glGetAttachedShaders",
    "glGetProgramInfoLog",
    "glGetProgramiv",
    "glGetShaderInfoLog",
    "glGetShaderSource",
    "glGetShaderiv",
    "glLinkProgram",
    "glReleaseShaderCompiler",
    "glShaderBinary",
    "glShaderSource",
    "glUseProgram",
    # Shader Attributes
    "glGetAttribLocation",
    "glBindAttribLocation",
    "glEnableVertexAttribArray",
    "glDisableVertexAttribArray",
    "glVertexAttribPointer",
    "glGetActiveAttrib",
    "glGetActiveUniform",
    # Shader Uniforms
    "glGetUniformLocation",
    "glGetUniformiv",
    "glUniform1fv",
    "glUniform1i",
    "glUniform1iv",
    "glUniform1uiv",
    "glUniform2fv",
    "glUniform2iv",
    "glUniform2uiv",
    "glUniform3fv",
    "glUniform3iv",
    "glUniform3uiv",
    "glUniform4fv",
    "glUniform4iv",
    "glUniform4uiv",
    "glUniformMatrix2fv",
    "glUniformMatrix3fv",
    "glUniformMatrix4fv",
    # Stencil
    "glStencilFunc",
    "glStencilFuncSeparate",
    "glStencilMask",
    "glStencilMaskSeparate",
    "glStencilOp",
    "glStencilOpSeparate",
    # Face Culling
    "glCullFace",
    # Front Face
    "glFrontFace",
    # Sync
    "glFlush",
    "glFinish",
    # Vertex Arrays
    "glBindVertexArray",
    "glDeleteVertexArrays",
    "glGenVertexArrays",
    # Uniform Block Object
    "glGetUniformBlockIndex",
    "glUniformBlockBinding",
    "glGetActiveUniformBlockName",
    "glGetActiveUniformBlockiv",
    # Sync object
    "glFenceSync",
    "glWaitSync",
    "glClientWaitSync",
    "glDeleteSync",
    # Read/Draw Buffer
    "glReadBuffer",
    "glDrawBuffers",
    # Clear Buffer
    "glClearBufferfv",
    "glClearBufferfi",
    #  Map buffer range
    "glMapBufferRange",
    "glUnmapBuffer",
] + cmds_optional


def get_proto_elems(xml_node):
    elems = []
    for text in xml_node.itertext():
        text = text.strip()
        if not text:
            continue
        elems.append(text)
    return elems


def gen(gl_xml, func_file, def_file):
    do_not_edit = "/* DO NOT EDIT - This file is autogenerated */\n"

    glfunctions = do_not_edit + textwrap.dedent(
        """
        #ifndef GLFUNCTIONS_H
        #define GLFUNCTIONS_H

        #include "glincludes.h"

        struct glfunctions {
        """
    )

    gldefinitions = do_not_edit + textwrap.dedent(
        """
        /* WARNING: this file must only be included once */

        #include <stddef.h>
        #include <stdint.h>

        #include "glcontext.h"

        #define M (1U << 0)

        static const struct gldefinition {
            const char *name;
            size_t offset;
            uint32_t flags;
        } gldefinitions[] = {
        """
    )

    xml = ET.parse(gl_xml)
    root = xml.getroot()
    commands = root.find("commands")
    for cmd in commands:
        proto = cmd.find("proto")
        funcname = proto.find("name").text

        if funcname not in cmds:
            continue

        ptype = proto.find("ptype")
        funcret = " ".join(get_proto_elems(proto)[:-1])

        func_args_specs = []
        func_args = []
        for param in cmd.findall("param"):
            func_args_specs.append(" ".join(get_proto_elems(param)))
            func_args.append(param.find("name").text)

        wrapper_args_specs = ["const struct glcontext *gl"] + func_args_specs

        data = {
            "func_ret": funcret,
            "func_name": funcname,
            "func_name_nogl": funcname[2:],  # with "gl" stripped
            "wrapper_args_specs": ", ".join(wrapper_args_specs),
            "func_args_specs": ", ".join(func_args_specs),
            "func_args": ", ".join(func_args),
            "flags": "0" if funcname in cmds_optional else "M",
        }

        glfunctions += "    %(func_ret)s (NGLI_GL_APIENTRY *%(func_name_nogl)s)(%(func_args_specs)s);\n" % data
        gldefinitions += '    {"%(func_name)s", offsetof(struct glfunctions, %(func_name_nogl)s), %(flags)s},\n' % data

        cmds.pop(cmds.index(funcname))
        if not cmds:
            break

    if cmds:
        print("WARNING: function(s) not found: " + ", ".join(cmds))

    glfunctions += "};\n\n#endif\n"
    gldefinitions += "};\n"

    with open(func_file, "w", newline="\n") as f:
        f.write(glfunctions)
    with open(def_file, "w", newline="\n") as f:
        f.write(gldefinitions)


if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: %s <gl.xml> <func.h> <def.h>" % sys.argv[0])
        sys.exit(1)
    gen(*sys.argv[1:])
