#
# Copyright 2020 GoPro Inc.
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

project(
  'libnodegl',
  'c',
   default_options: ['c_std=c99'],
   license: 'Apache',
   meson_version: '>= 0.57.0',
   version: files('VERSION'),
)

conf_data = configuration_data()
version_array = meson.project_version().split('.')
conf_data.set('version_major', version_array[0])
conf_data.set('version_minor', version_array[1])
conf_data.set('version_micro', version_array[2])

host_system = host_machine.system()
cpu_family = host_machine.cpu_family()
cc = meson.get_compiler('c')

# meson currently (0.56.0) doesn't distinguish between macOS and iOS (the
# "darwin" identifier is shared), so we need to test explicitly.
# We are also checking for IPHONE and not IOS but since the latter is actually
# more specific (doesn't include tvOS and watchOS) we may want to consider
# switching to it at some point.
# See https://github.com/mesonbuild/meson/issues/7944 for more information
iphone_check = '''#include <TargetConditionals.h>
#if !TARGET_OS_IPHONE
#error not iphone
#endif'''
if host_system == 'darwin' and cc.compiles(iphone_check, name: 'iPhone target')
  host_system = 'iphone'
endif

install_rpath = get_option('rpath') ? get_option('prefix') / get_option('libdir') : ''

debug_opts = get_option('debug_opts')
conf_data.set10('TARGET_' + host_system.to_upper(), true)
conf_data.set10('ARCH_' + cpu_family.to_upper(), true)
conf_data.set10('CONFIG_SMALL', get_option('small'))
conf_data.set10('DEBUG_GL', 'gl' in debug_opts)
conf_data.set10('DEBUG_VK', 'vk' in debug_opts)
conf_data.set10('DEBUG_MEM', 'mem' in debug_opts)
conf_data.set10('DEBUG_SCENE', 'scene' in debug_opts)
conf_data.set10('DEBUG_GPU_CAPTURE', 'gpu_capture' in debug_opts)

if host_system == 'windows'
  if cc.get_id() == 'msvc'
    conf_data.set10('TARGET_MSVC', true)
    msvc_args = [
      # Ignore several spurious warnings
      '/wd4018', # comparison between signed and unsigned numbers
      '/wd4146', # unary minus operator applied to unsigned type when its result is still unsigned (INT_MIN)
      '/wd4244', # conversion from 'type1' to 'type2', resulting in a possible loss of data
      '/wd4305', # value is converted to a smaller type in an initialization, resulting in a loss of information.
      # Enable some warnings to match GCC/Clang
      '/w14062', # enumerator 'identifier' in switch of enum 'enumeration' is not handled
      '/w14101', # 'identifier' : unreferenced local variable
      '/w14189', # 'identifier' : local variable is initialized but not referenced
    ]
    add_project_arguments(msvc_args, language: 'c')
  else
    conf_data.set10('TARGET_MINGW_W64', true)
  endif
endif

# This trim prefix is used to make __FILE__ starts from the source dir with
# out-of-tree builds.
# Adjusted from https://github.com/mesonbuild/meson/issues/7485
trim_prefix = run_command([
  find_program('python'), '-c', 'import sys,os;print(os.path.relpath(*sys.argv[1:3]))',
  meson.current_source_dir(),
  meson.project_build_root(),
], check: true).stdout().strip()

project_args = cc.get_supported_arguments([
  '-Werror=missing-prototypes',
  '-Werror=pointer-arith',
  '-Werror=vla',
  '-Wformat=2',
  '-Wignored-qualifiers',
  '-Wimplicit-fallthrough',
  '-Wlogical-op',
  '-fmacro-prefix-map=@0@/='.format(trim_prefix),
])
add_project_arguments(project_args, language: 'c')

# Also apply to local generator targets such as gen_doc
# See https://github.com/mesonbuild/meson/issues/7940
add_project_arguments(project_args, language: 'c', native: true)


#
# Library main configuration
#

lib_version = '0.0.0'
lib_src = files(
  'animation.c',
  'api.c',
  'blending.c',
  'block.c',
  'bstr.c',
  'buffer.c',
  'colorconv.c',
  'darray.c',
  'deserialize.c',
  'dot.c',
  'drawutils.c',
  'eval.c',
  'filterschain.c',
  'format.c',
  'geometry.c',
  'gpu_ctx.c',
  'hmap.c',
  'hud.c',
  'hwconv.c',
  'hwmap.c',
  'hwmap_common.c',
  'image.c',
  'log.c',
  'math_utils.c',
  'memory.c',
  'node_animatedbuffer.c',
  'node_animated.c',
  'node_animkeyframe.c',
  'node_block.c',
  'node_buffer.c',
  'node_camera.c',
  'node_circle.c',
  'node_compute.c',
  'node_computeprogram.c',
  'node_filters.c',
  'node_geometry.c',
  'node_graphicconfig.c',
  'node_group.c',
  'node_identity.c',
  'node_io.c',
  'node_eval.c',
  'node_media.c',
  'node_noise.c',
  'node_path.c',
  'node_pathkey.c',
  'node_program.c',
  'node_quad.c',
  'node_render.c',
  'node_renderother.c',
  'node_resourceprops.c',
  'node_rotate.c',
  'node_rotatequat.c',
  'node_rtt.c',
  'node_scale.c',
  'node_skew.c',
  'node_smoothpath.c',
  'node_streamed.c',
  'node_streamedbuffer.c',
  'node_text.c',
  'node_texture.c',
  'node_textureview.c',
  'node_time.c',
  'node_timerangefilter.c',
  'node_timerangemodes.c',
  'node_transform.c',
  'node_translate.c',
  'node_triangle.c',
  'node_uniform.c',
  'node_userselect.c',
  'node_userswitch.c',
  'node_velocity.c',
  'nodes.c',
  'noise.c',
  'params.c',
  'pass.c',
  'path.c',
  'pgcache.c',
  'pgcraft.c',
  'pipeline.c',
  'pipeline_compat.c',
  'precision.c',
  'program.c',
  'rendertarget.c',
  'rnode.c',
  'serialize.c',
  'texture.c',
  'transforms.c',
  'type.c',
  'utils.c',
)

if host_machine.cpu_family() == 'aarch64'
  lib_src += files('asm_aarch64.S')
endif
have_x86_intr = host_machine.cpu_family().startswith('x86') and cc.has_header('immintrin.h')
if have_x86_intr
  lib_src += files('simd_x86.c')
  conf_data.set10('HAVE_X86_INTR', true)
endif

hosts_cfg = {
  'linux': {
    'deps': ['x11'],
  },
  'darwin': {
    'frameworks': ['CoreVideo', 'CoreFoundation', 'AppKit', 'IOSurface'],
  },
  'android': {
    'deps': ['libavcodec'],
    'libs': ['android'],
    'src': files(
      'jni_utils.c',
      'android_ctx.c',
      'android_utils.c',
      'android_looper.c',
      'android_surface.c',
      'android_handler.c',
      'android_handlerthread.c',
      'android_imagereader.c',
    ),
  },
  'iphone': {
    'frameworks': ['CoreVideo', 'Foundation'],
  },
}

lib_deps = [
  cc.find_library('m', required: false),
  dependency('libsxplayer', version: '>= 9.12.0'),
  dependency('threads'),
]

host_cfg = hosts_cfg.get(host_system, {})
foreach dep : host_cfg.get('deps', [])
  lib_deps += dependency(dep)
endforeach
foreach dep : host_cfg.get('libs', [])
  lib_deps += cc.find_library(dep)
endforeach
if 'frameworks' in host_cfg
  lib_deps += dependency('appleframeworks', modules: host_cfg.get('frameworks'))
endif
lib_src += host_cfg.get('src', [])


#
# VAAPI dependency
#

libva_version = '>= 1.1.0'
vaapi_enabled = false

opt_wayland = get_option('wayland')
dep_wayland_client = dependency('wayland-client', required: opt_wayland)
dep_wayland_egl    = dependency('wayland-egl',    required: opt_wayland)
if dep_wayland_client.found() and dep_wayland_egl.found()
  lib_deps += [dep_wayland_client, dep_wayland_egl]
  conf_data.set10('HAVE_WAYLAND', true)
endif

opt_vaapi_x11 = get_option('vaapi-x11')
dep_libva_x11 = dependency('libva-x11', version: libva_version, required: opt_vaapi_x11)
dep_libva_x11_drm = dependency('libva-drm', version: libva_version, required: opt_vaapi_x11)
if dep_libva_x11.found() and dep_libva_x11_drm.found()
  vaapi_enabled = true
  lib_deps += [dep_libva_x11, dep_libva_x11_drm]
  conf_data.set10('HAVE_VAAPI_X11', true)
endif

opt_vaapi_wayland = get_option('vaapi-wayland')
dep_libva_wayland = dependency('libva-wayland', version: libva_version, required: opt_vaapi_wayland)
dep_libva_wayland_drm = dependency('libva-drm', version: libva_version, required: opt_vaapi_wayland)
if dep_libva_wayland.found() and dep_libva_wayland_drm.found()
  vaapi_enabled = true
  lib_deps += [dep_libva_wayland, dep_libva_wayland_drm]
  conf_data.set10('HAVE_VAAPI_WAYLAND', true)
endif

if vaapi_enabled
  lib_src += files('vaapi_ctx.c')
  conf_data.set10('HAVE_VAAPI', true)
endif


#
# GPU Capture Options
#

if 'gpu_capture' in debug_opts
  renderdoc_dir = get_option('renderdoc_dir')
  renderdoc_include_dir = ''
  if host_system == 'windows'
    renderdoc_include_dir = renderdoc_dir
  else
    renderdoc_include_dir = renderdoc_dir / 'include'
  endif
  if cc.has_header(renderdoc_include_dir / 'renderdoc_app.h', required: true)
    lib_src += files('gpu_capture_renderdoc.c')
    lib_deps += declare_dependency(include_directories: renderdoc_include_dir)
    lib_deps += cc.find_library('dl', required: host_system == 'linux')
  endif
endif


#
# Graphic backend dependencies
#

gbackends_cfg = {
  'gl': {
    'src': files(
      'backends/gl/api_gl.c',
      'backends/gl/buffer_gl.c',
      'backends/gl/format_gl.c',
      'backends/gl/gpu_ctx_gl.c',
      'backends/gl/glcontext.c',
      'backends/gl/glstate.c',
      'backends/gl/hwmap_gl.c',
      'backends/gl/pipeline_gl.c',
      'backends/gl/program_gl.c',
      'backends/gl/program_gl_utils.c',
      'backends/gl/rendertarget_gl.c',
      'backends/gl/texture_gl.c',
      'backends/gl/topology_gl.c',
      'backends/gl/type_gl.c',
    ),
    'cfg': 'BACKEND_GL',
    'hosts_cfg': {
      'linux': {
        'src': files('backends/gl/glcontext_egl.c'),
        'cfg': 'HAVE_GLPLATFORM_EGL',
        'deps': ['gl', 'egl'],
      },
      'darwin': {
        'src': files('backends/gl/glcontext_nsgl.m', 'backends/gl/hwmap_videotoolbox_darwin_gl.c'),
        'cfg': 'HAVE_GLPLATFORM_NSGL',
        'frameworks': ['OpenGL'],
      },
      'android': {
        'src': files('backends/gl/glcontext_egl.c', 'backends/gl/hwmap_mediacodec_gl.c'),
        'cfg': 'HAVE_GLPLATFORM_EGL',
        'libs': ['EGL'],
      },
      'iphone': {
        'src': files('backends/gl/glcontext_eagl.m', 'backends/gl/hwmap_videotoolbox_ios_gl.c'),
        'cfg': 'HAVE_GLPLATFORM_EAGL',
        'frameworks': ['OpenGLES'],
      },
      'windows': {
        'src': files('backends/gl/glcontext_wgl.c'),
        'cfg': 'HAVE_GLPLATFORM_WGL',
        'libs': ['OpenGL32', 'gdi32'],
      },
    },
  },
  'vk': {
    'src': files(
      'backends/vk/api_vk.c',
      'backends/vk/buffer_vk.c',
      'backends/vk/command_vk.c',
      'backends/vk/format_vk.c',
      'backends/vk/gpu_ctx_vk.c',
      'backends/vk/hwmap_vk.c',
      'backends/vk/pipeline_vk.c',
      'backends/vk/program_vk.c',
      'backends/vk/rendertarget_vk.c',
      'backends/vk/texture_vk.c',
      'backends/vk/vkcontext.c',
      'backends/vk/vkutils.c',
      'backends/vk/glslang_utils.c',
      'backends/vk/ycbcr_sampler_vk.c',
    ),
    'cfg': 'BACKEND_VK',
    'hosts_cfg': {
      'linux': {
        'deps': ['vulkan', 'glslang'],
      },
      'darwin': {
        'src': files('backends/vk/hwmap_videotoolbox_darwin_vk.m'),
        'deps': ['glslang'],
        'libs': ['MoltenVK'],
        'frameworks': ['Metal'],
      },
      'android': {
        'src': files('backends/vk/hwmap_mediacodec_vk.c'),
        'deps': ['vulkan', 'glslang'],
      },
      'iphone': {
        'src': files('backends/vk/hwmap_videotoolbox_darwin_vk.m'),
        'deps': ['glslang'],
        'libs': ['MoltenVK'],
        'frameworks': ['Metal'],
      },
      'windows': {
        'deps': ['vulkan', 'glslang'],
      },
    },
  },
}

if host_system in ['darwin', 'iphone']
  add_languages('objc')
endif

# Additional search paths for find_library
extra_library_dirs = get_option('extra_library_dirs')
extra_include_dirs = get_option('extra_include_dirs')

foreach gbackend_name, gbackend_cfg : gbackends_cfg
  opt_gbackend = get_option('gbackend-' + gbackend_name)
  if opt_gbackend.disabled()
    continue
  endif
  hosts_backend_cfg = gbackend_cfg.get('hosts_cfg')
  if host_system not in hosts_backend_cfg
    continue
  endif
  host_backend_cfg = hosts_backend_cfg.get(host_system)
  gbackend_deps = []
  foreach dep : host_backend_cfg.get('deps', [])
    if dep == 'glslang'
      add_languages('cpp')
      cxx = meson.get_compiler('cpp')
      # glslang can be distributed as shared or static libraries optionally
      # linked to SPIRV-Tools:
      # - on Linux, it is generally distributed as shared libraries, more
      #   specifically, libglslang.so, libHLSL.so, libSPVRemapper.so and
      #   libSPIRV.so are installed. libOGLCompiler.a and libOSDependant.a are
      #   also provided but not required by the shared libraries. There is no
      #   dependency between the shared libraries.
      # - on the other platforms (macOS, Windows, Android, iOS) it is generally
      #   distributed or installed as static libraries, more specifically,
      #   libglslang.a, libMachineIndependent.a, libOSDependent.a, libHLSL.a,
      #   libOGLCompiler.a, libGenericCodeGen.a, libSPVRemapper.a, libSPIRV.a
      #   libSPIRV-Tools-opt.a (optional), libSPIRV-Tools.a (optional) are
      #   installed.
      # The following list declares the glslang libraries that we need to check
      # and link against in the right order (to allow static linking). Some
      # libraries are marked as non-required because they might be present or
      # not depending on the build of glslang.
      glslang_libs = [
        {'name': 'glslang',                         'required': true},
        {'name': 'glslang-default-resource-limits', 'required': true},
        {'name': 'MachineIndependent',              'required': false},
        {'name': 'OSDependent',                     'required': true},
        {'name': 'HLSL',                            'required': true},
        {'name': 'OGLCompiler',                     'required': true},
        {'name': 'GenericCodeGen',                  'required': false},
        {'name': 'SPVRemapper',                     'required': true},
        {'name': 'SPIRV',                           'required': true},
        {'name': 'SPIRV-Tools-opt',                 'required': false},
        {'name': 'SPIRV-Tools',                     'required': false},
      ]
      foreach lib : glslang_libs
        libname = lib.get('name')
        # Only require the library if the backend is explicitly enabled by the
        # user. This makes meson bail out early if the required library can not
        # be found.
        required = lib.get('required') and opt_gbackend.enabled()
        dep = cxx.find_library(libname, required: required, dirs: extra_library_dirs)
        # Add the dependency if the library is marked as required or if it has
        # been found so the backend can be disabled later on.
        if lib.get('required') or dep.found()
          gbackend_deps += dep
        endif
      endforeach
      glslang_header = 'glslang/build_info.h'
      glslang_include_dirs = include_directories(extra_include_dirs)
      if cxx.has_header(glslang_header, include_directories: glslang_include_dirs)
        glslang_ver_major = cxx.get_define(
          'GLSLANG_VERSION_MAJOR',
          prefix: '#include <' + glslang_header + '>',
          include_directories: glslang_include_dirs,
        ).to_int()
        glslang_ver_minor = cxx.get_define(
          'GLSLANG_VERSION_MINOR',
          prefix: '#include <' + glslang_header + '>',
          include_directories: glslang_include_dirs,
        ).to_int()
        glslang_ver = '@0@.@1@'.format(
            glslang_ver_major,
            glslang_ver_minor,
        )
        glslang_min_ver = '>= 11.1.0'
        if glslang_ver.version_compare(glslang_min_ver)
          gbackend_deps += declare_dependency(include_directories: glslang_include_dirs)
        else
          if opt_gbackend.enabled()
            error('detected glslang version @0@ is too old, must be @1@'.format(
                  glslang_ver, glslang_min_ver))
          else
            gbackend_deps += disabler()
          endif
        endif
      else
        if opt_gbackend.enabled()
          error('glslang header not found')
        else
          gbackend_deps += disabler()
        endif
      endif
    else
      gbackend_deps += dependency(dep, required: opt_gbackend)
    endif
  endforeach
  foreach lib : host_backend_cfg.get('libs', [])
    # We need to call find_library() twice because specifying the dirs
    # parameter inhibits system library detection.
    # See: https://github.com/mesonbuild/meson/issues/10120
    dep = cc.find_library(lib, required: false)
    if not dep.found()
      dep = cc.find_library(lib, required: opt_gbackend, dirs: extra_library_dirs)
      if dep.found()
        gbackend_deps += declare_dependency(include_directories: extra_include_dirs)
      endif
    endif
    gbackend_deps += dep
  endforeach
  if 'frameworks' in host_backend_cfg
    gbackend_deps += dependency('appleframeworks', modules: host_backend_cfg.get('frameworks'), required: opt_gbackend)
  endif

  all_dep_found = true
  foreach dep : gbackend_deps
    all_dep_found = all_dep_found and dep.found()
  endforeach
  message('backend: @0@ enabled: @1@'.format(gbackend_name, all_dep_found.to_string()))
  if all_dep_found

    if gbackend_name == 'gl' and vaapi_enabled
      lib_src += files('backends/gl/hwmap_vaapi_gl.c')
    endif

    if gbackend_name == 'vk' and vaapi_enabled
      lib_src += files('backends/vk/hwmap_vaapi_vk.c')
    endif

    lib_src += gbackend_cfg.get('src')
    lib_src += host_backend_cfg.get('src', [])
    lib_deps += gbackend_deps
    conf_data.set10(gbackend_cfg.get('cfg'), true)
    if 'cfg' in host_backend_cfg
      conf_data.set10(host_backend_cfg.get('cfg'), true)
    endif
  endif
endforeach


#
# GLSL to C (header)
#
shaders = {
  'filter_alpha.glsl': 'filter_alpha.h',
  'filter_contrast.glsl': 'filter_contrast.h',
  'filter_exposure.glsl': 'filter_exposure.h',
  'filter_inversealpha.glsl': 'filter_inversealpha.h',
  'filter_linear2srgb.glsl': 'filter_linear2srgb.h',
  'filter_opacity.glsl': 'filter_opacity.h',
  'filter_premult.glsl': 'filter_premult.h',
  'filter_saturation.glsl': 'filter_saturation.h',
  'filter_srgb2linear.glsl': 'filter_srgb2linear.h',
  'hdr_hlg2sdr.frag': 'hdr_hlg2sdr_frag.h',
  'helper_linear2srgb.glsl': 'helper_linear2srgb_glsl.h',
  'helper_srgb2linear.glsl': 'helper_srgb2linear_glsl.h',
  'helper_misc_utils.glsl': 'helper_misc_utils_glsl.h',
  'source_color.frag': 'source_color_frag.h',
  'source_color.vert': 'source_color_vert.h',
  'source_gradient.frag': 'source_gradient_frag.h',
  'source_gradient.vert': 'source_gradient_vert.h',
  'source_gradient4.frag': 'source_gradient4_frag.h',
  'source_gradient4.vert': 'source_gradient4_vert.h',
  'source_texture.frag': 'source_texture_frag.h',
  'source_texture.vert': 'source_texture_vert.h',
}
glsl2c = find_program('scripts/glsl2c.py')
shaders_h = []
foreach shader_file, header_file : shaders
  shaders_h += custom_target(
    shader_file,
    command: [glsl2c, '@INPUT@'],
    capture: true,
    input: files('glsl' / shader_file),
    output: header_file,
  )
endforeach
lib_src += shaders_h


#
# Library
#

configure_file(output: 'config.h', configuration: conf_data)

lib_c_args = []
if get_option('default_library') == 'shared'
  lib_c_args += '-DBUILD_NODEGL_SHARED_LIB'
else
  lib_c_args += '-DUSE_NODEGL_STATIC_LIB'
endif

lib_objc_args = []
if host_system == 'darwin'
  lib_c_args += '-DGL_SILENCE_DEPRECATION'
  lib_objc_args += '-DGL_SILENCE_DEPRECATION'
elif host_system == 'iphone'
  lib_c_args += '-DGLES_SILENCE_DEPRECATION'
  lib_objc_args += '-DGLES_SILENCE_DEPRECATION'
endif

libnodegl = library(
  'nodegl',
  lib_src,
  dependencies: lib_deps,
  install: true,
  install_rpath: install_rpath,
  version: meson.project_version(),
  c_args: lib_c_args,
  objc_args: lib_objc_args,
  gnu_symbol_visibility: 'hidden',
)
lib_header = configure_file(
  input: files('nodegl.h.in'),
  output: 'nodegl.h',
  configuration: conf_data
)
install_headers(lib_header)

pkg = import('pkgconfig')
pkg_extra_cflags = []

if get_option('default_library') == 'static'
  pkg_extra_cflags += '-DUSE_NODEGL_STATIC_LIB'
endif

pkg.generate(
  libnodegl,
  name: 'libnodegl',  # not specifying the name would fallback on "nodegl.pc" instead of "libnodegl.pc"
  description: 'Node/Graph based graphics engine',
  extra_cflags: pkg_extra_cflags,
  variables: ['datarootdir=${prefix}/share'],
)


#
# Specifications
#

copyfile = find_program('scripts/copyfile.py')
specs_filename = 'nodes.specs'
dest_datadir = get_option('datadir') / 'nodegl'
install_data(files(specs_filename), install_dir: dest_datadir)
install_data(files('android/java/OnFrameAvailableListener.java'), install_dir: dest_datadir / 'java/org/nodegl')

# XXX: should we use an intermediate static_library() to share the objects
# between the library and these tools?
# https://git.archlinux.org/pacman.git/tree/meson.build?id=4533c6a8e0f39c7707e671b7f9687607b46f1417#n310
# seem to imply some extract_all_objects(recursive: true)
gen_specs = executable(
  'gen_specs',
  lib_src + files('gen_specs.c'),
  dependencies: lib_deps,
  build_by_default: false,
  native: true,
)
specs_file = custom_target(
  specs_filename,
  command: gen_specs,
  capture: true,
  output: specs_filename,
)
run_target(
  'updatespecs',
  command: [copyfile, specs_file, meson.current_source_dir()],
)


#
# Doc
#

if not get_option('small')
  gen_doc = executable(
    'gen_doc',
    lib_src + files('gen_doc.c'),
    dependencies: lib_deps,
    build_by_default: false,
    native: true,
  )
  doc_file = custom_target(
    'libnodegl.md',
    command: gen_doc,
    capture: true,
    output: 'libnodegl.md',
  )
  run_target(
    'updatedoc',
    command: [copyfile, doc_file, meson.current_source_dir() / 'doc'],
  )
endif


#
# OpenGL specifications
#

gl_xml = custom_target(
  'gl.xml',
  command: [find_program('curl'), 'https://raw.githubusercontent.com/KhronosGroup/OpenGL-Registry/master/xml/gl.xml', '-o', '@OUTPUT@'],
  capture: false,
  output: 'gl.xml',
)
gl_generated_files = custom_target(
  'gl_generated_files',
  command: [find_program('scripts/gen-gl-wrappers.py'), '@INPUT@', '@OUTPUT@'],
  capture: false,
  input: gl_xml,
  output: ['glfunctions.h', 'gldefinitions_data.h', 'glwrappers.h'],
)
run_target(
  'updateglwrappers',
  command: [
    copyfile,
    # FIXME: In 0.56.0, we get "WARNING: custom_target 'gl_generated_files' has
    # more than one output!  Using the first one.", so we need to do this
    # instead of just using gl_generated_files. It also requires the "depends:"
    # entry below.
    # See https://github.com/mesonbuild/meson/issues/7891 for more information
    gl_generated_files[0].full_path(),
    gl_generated_files[1].full_path(),
    gl_generated_files[2].full_path(),
    meson.current_source_dir() / 'backends/gl'
  ],
  depends: gl_generated_files,
)


#
# Tests
#

test_asm_src = files('test_asm.c', 'math_utils.c')
if host_machine.cpu_family() == 'aarch64'
  test_asm_src += files('asm_aarch64.S')
endif
if have_x86_intr
  test_asm_src += files('simd_x86.c')
endif

test_progs = {
  'Assembly': {
    'exe': 'test_asm',
    'src': test_asm_src,
  },
  'Color convertion': {
    'exe': 'test_colorconv',
    'src': files('test_colorconv.c', 'colorconv.c', 'log.c', 'memory.c'),
  },
  'Dynamic array': {
    'exe': 'test_darray',
    'src': files('test_darray.c', 'darray.c', 'memory.c'),
  },
  'Draw utils': {
    'exe': 'test_draw',
    'src': files('test_draw.c', 'drawutils.c', 'memory.c'),
    'args': ['ngl-test.ppm']
  },
  'Eval': {
    'exe': 'test_eval',
    'src': files('test_eval.c', 'eval.c', 'darray.c', 'memory.c', 'hmap.c', 'bstr.c', 'log.c', 'utils.c'),
  },
  'Hash map': {
    'exe': 'test_hmap',
    'src': files('test_hmap.c', 'bstr.c', 'log.c', 'utils.c', 'memory.c'),
  },
  'Noise': {
    'exe': 'test_noise',
    'src': files('test_noise.c', 'noise.c', 'log.c', 'memory.c'),
  },
  'Path': {
    'exe': 'test_path',
    'src': files('test_path.c', 'darray.c', 'path.c', 'log.c', 'memory.c', 'math_utils.c'),
  },
  'Utils': {
    'exe': 'test_utils',
    'src': files('test_utils.c', 'bstr.c', 'log.c', 'utils.c', 'memory.c'),
  },
}

if get_option('tests')
  foreach test_key, test_data : test_progs
    exe = executable(
      test_data.get('exe'),
      test_data.get('src'),
      dependencies: lib_deps,
      build_by_default: false,
      install: false,
    )
    test(test_key, exe, args: test_data.get('args', []))
  endforeach
endif
