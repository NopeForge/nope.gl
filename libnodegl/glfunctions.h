/*
 * Copyright 2017 GoPro Inc.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#define M (1 << 0)

/* Enable */
NGLI_GL_FUNC(M, void, Enable, GLenum cap)
NGLI_GL_FUNC(M, void, Disable, GLenum cap)

/* Error */
NGLI_GL_FUNC(M, GLenum, GetError, void)

/* Get */
NGLI_GL_FUNC(M, void, GetBooleanv, GLenum pname, GLboolean *data)
NGLI_GL_FUNC(M, void, GetIntegerv, GLenum pname, GLint *data)
NGLI_GL_FUNC(M, const GLubyte*, GetString, GLenum name)
NGLI_GL_FUNC(M, const GLubyte*, GetStringi, GLenum name, GLuint index)

/* Viewport */
NGLI_GL_FUNC(M, void, Viewport, GLint x, GLint y, GLsizei width, GLsizei height)

/* Color */
NGLI_GL_FUNC(M, void, ColorMask, GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)

/* Clear */
NGLI_GL_FUNC(M, void, Clear, GLbitfield mask)
NGLI_GL_FUNC(M, void, ClearColor, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)

/* Blending */
NGLI_GL_FUNC(M, void, BlendColor, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
NGLI_GL_FUNC(M, void, BlendEquation, GLenum mode)
NGLI_GL_FUNC(M, void, BlendEquationSeparate, GLenum modeRGB, GLenum modeAlpha)
NGLI_GL_FUNC(M, void, BlendFunc, GLenum sfactor, GLenum dfactor)
NGLI_GL_FUNC(M, void, BlendFuncSeparate, GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha)

/* Draw */
NGLI_GL_FUNC(M, void, DrawElements, GLenum mode, GLsizei count, GLenum type, const void *indices)

/* Texture */
NGLI_GL_FUNC(M, void, ActiveTexture, GLenum texture)
NGLI_GL_FUNC(M, void, BindTexture, GLenum target, GLuint texture)
NGLI_GL_FUNC(M, void, DeleteTextures, GLsizei n, const GLuint *textures)
NGLI_GL_FUNC(M, void, GenTextures, GLsizei n, GLuint *textures)
NGLI_GL_FUNC(M, void, GenerateMipmap, GLenum target)
NGLI_GL_FUNC(M, void, TexImage2D, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels)
NGLI_GL_FUNC(M, void, TexParameteri, GLenum target, GLenum pname, GLint param)
NGLI_GL_FUNC(M, void, TexSubImage2D, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels)

/* Framebuffer */
NGLI_GL_FUNC(M, GLenum, CheckFramebufferStatus, GLenum target)
NGLI_GL_FUNC(M, void, BindFramebuffer, GLenum target, GLuint framebuffer)
NGLI_GL_FUNC(0, void, BlitFramebuffer, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
NGLI_GL_FUNC(M, void, DeleteFramebuffers, GLsizei n, const GLuint *framebuffers)
NGLI_GL_FUNC(M, void, FramebufferRenderbuffer, GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
NGLI_GL_FUNC(M, void, FramebufferTexture2D, GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
NGLI_GL_FUNC(M, void, GenFramebuffers, GLsizei n, GLuint *framebuffers)
NGLI_GL_FUNC(M, void, ReadPixels, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels)

/* Buffer */
NGLI_GL_FUNC(M, void, BindBuffer, GLenum target, GLuint buffer)
NGLI_GL_FUNC(M, void, BufferData, GLenum target, GLsizeiptr size, const void *data, GLenum usage)
NGLI_GL_FUNC(M, void, DeleteBuffers, GLsizei n, const GLuint *buffers)
NGLI_GL_FUNC(M, void, GenBuffers, GLsizei n, GLuint *buffers)

/* Render buffer */
NGLI_GL_FUNC(M, void, BindRenderbuffer, GLenum target, GLuint renderbuffer)
NGLI_GL_FUNC(M, void, DeleteRenderbuffers, GLsizei n, const GLuint *renderbuffers)
NGLI_GL_FUNC(M, void, GenRenderbuffers, GLsizei n, GLuint *renderbuffers)
NGLI_GL_FUNC(M, void, GetRenderbufferParameteriv, GLenum target, GLenum pname, GLint *params)
NGLI_GL_FUNC(M, void, RenderbufferStorage, GLenum target, GLenum internalformat, GLsizei width, GLsizei height)

/* Shader */
NGLI_GL_FUNC(M, GLuint, CreateProgram, void)
NGLI_GL_FUNC(M, GLuint, CreateShader, GLenum type)
NGLI_GL_FUNC(M, void, AttachShader, GLuint program, GLuint shader)
NGLI_GL_FUNC(M, void, CompileShader, GLuint shader)
NGLI_GL_FUNC(M, void, DeleteProgram, GLuint program)
NGLI_GL_FUNC(M, void, DeleteShader, GLuint shader)
NGLI_GL_FUNC(M, void, DetachShader, GLuint program, GLuint shader)
NGLI_GL_FUNC(M, void, GetAttachedShaders, GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders)
NGLI_GL_FUNC(M, void, GetProgramInfoLog, GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog)
NGLI_GL_FUNC(M, void, GetProgramiv, GLuint program, GLenum pname, GLint *params)
NGLI_GL_FUNC(M, void, GetShaderInfoLog, GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog)
NGLI_GL_FUNC(M, void, GetShaderSource, GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source)
NGLI_GL_FUNC(M, void, GetShaderiv, GLuint shader, GLenum pname, GLint *params)
NGLI_GL_FUNC(M, void, LinkProgram, GLuint program)
NGLI_GL_FUNC(M, void, ReleaseShaderCompiler, void)
NGLI_GL_FUNC(M, void, ShaderBinary, GLsizei count, const GLuint *shaders, GLenum binaryformat, const void *binary, GLsizei length)
NGLI_GL_FUNC(M, void, ShaderSource, GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length)
NGLI_GL_FUNC(M, void, UseProgram, GLuint program)

/* Shader Attributes */
NGLI_GL_FUNC(M, GLint, GetAttribLocation, GLuint program, const GLchar *name)
NGLI_GL_FUNC(M, void, BindAttribLocation, GLuint program, GLuint index, const GLchar *name)
NGLI_GL_FUNC(M, void, EnableVertexAttribArray, GLuint index)
NGLI_GL_FUNC(M, void, VertexAttribPointer, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer)

/* Shader Uniforms */
NGLI_GL_FUNC(M, GLint, GetUniformLocation, GLuint program, const GLchar *name)
NGLI_GL_FUNC(M, void, Uniform1f, GLint location, GLfloat v0)
NGLI_GL_FUNC(M, void, Uniform1fv, GLint location, GLsizei count, const GLfloat *value)
NGLI_GL_FUNC(M, void, Uniform1i, GLint location, GLint v0)
NGLI_GL_FUNC(M, void, Uniform1iv, GLint location, GLsizei count, const GLint *value)
NGLI_GL_FUNC(M, void, Uniform2f, GLint location, GLfloat v0, GLfloat v1)
NGLI_GL_FUNC(M, void, Uniform2fv, GLint location, GLsizei count, const GLfloat *value)
NGLI_GL_FUNC(M, void, Uniform2i, GLint location, GLint v0, GLint v1)
NGLI_GL_FUNC(M, void, Uniform2iv, GLint location, GLsizei count, const GLint *value)
NGLI_GL_FUNC(M, void, Uniform3f, GLint location, GLfloat v0, GLfloat v1, GLfloat v2)
NGLI_GL_FUNC(M, void, Uniform3fv, GLint location, GLsizei count, const GLfloat *value)
NGLI_GL_FUNC(M, void, Uniform3i, GLint location, GLint v0, GLint v1, GLint v2)
NGLI_GL_FUNC(M, void, Uniform3iv, GLint location, GLsizei count, const GLint *value)
NGLI_GL_FUNC(M, void, Uniform4f, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
NGLI_GL_FUNC(M, void, Uniform4fv, GLint location, GLsizei count, const GLfloat *value)
NGLI_GL_FUNC(M, void, Uniform4i, GLint location, GLint v0, GLint v1, GLint v2, GLint v3)
NGLI_GL_FUNC(M, void, Uniform4iv, GLint location, GLsizei count, const GLint *value)
NGLI_GL_FUNC(M, void, UniformMatrix2fv, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
NGLI_GL_FUNC(M, void, UniformMatrix3fv, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
NGLI_GL_FUNC(M, void, UniformMatrix4fv, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)

/* Vertex Arrays */
NGLI_GL_FUNC(0, void, BindVertexArray, GLuint array)
NGLI_GL_FUNC(0, void, DeleteVertexArrays, GLsizei n, const GLuint *arrays)
NGLI_GL_FUNC(0, void, GenVertexArrays, GLsizei n, GLuint *arrays)

/* Stencil */
NGLI_GL_FUNC(M, void, StencilFunc, GLenum func, GLint ref, GLuint mask)
NGLI_GL_FUNC(M, void, StencilFuncSeparate, GLenum face, GLenum func, GLint ref, GLuint mask)
NGLI_GL_FUNC(M, void, StencilMask, GLuint mask)
NGLI_GL_FUNC(M, void, StencilMaskSeparate, GLenum face, GLuint mask)
NGLI_GL_FUNC(M, void, StencilOp, GLenum fail, GLenum zfail, GLenum zpass)
NGLI_GL_FUNC(M, void, StencilOpSeparate, GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass)
