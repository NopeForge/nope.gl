#!/usr/bin/env python3
#
# Copyright 2023 Nope Forge
# Copyright 2021-2022 GoPro Inc.
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

import argparse
import glob
import hashlib
import logging
import os
import os.path as op
import pathlib
import platform
import shlex
import shutil
import stat
import sysconfig
import tarfile
import textwrap
import urllib.request
import venv
import zipfile
from multiprocessing import Pool
from subprocess import run

_ROOTDIR = op.abspath(op.dirname(__file__))
_SYSTEM = "MinGW" if sysconfig.get_platform().startswith("mingw") else platform.system()
_RENDERDOC_ID = f"renderdoc_{_SYSTEM}"
_EXTERNAL_DEPS = dict(
    ffmpeg_Windows=dict(
        version="6.0",
        url="https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2023-03-31-12-50/ffmpeg-n@VERSION@-11-g3980415627-win64-gpl-shared-@VERSION@.zip",
        dst_file="ffmpeg-@VERSION@.zip",
        sha256="13c3c2f0203136eff24bd15c1ee22c0b523efdc0ace59ef5e1f8abfcecf8c7df",
    ),
    nopemd=dict(
        version="11.1.1",
        url="https://github.com/NopeForge/nope.media/archive/v@VERSION@.tar.gz",
        dst_file="nope.media-@VERSION@.tar.gz",
        sha256="5a327e85a274baa3ed6e4e8c3e08d2bdc22d9cd32efaa5b956325032f8d2ce82",
    ),
    egl_registry=dict(
        version="9ab6036",
        url="https://github.com/KhronosGroup/EGL-Registry/archive/@VERSION@.zip",
        dst_file="egl-registry-@VERSION@.zip",
        sha256="b981b5a250d4f10284c670ae89f4a43c9eac42adca65306e5d56b921f609c46b",
    ),
    opengl_registry=dict(
        version="d0342a4",
        url="https://github.com/KhronosGroup/OpenGL-Registry/archive/@VERSION@.zip",
        dst_file="opengl-registry-@VERSION@.zip",
        sha256="8e6efa8d9ec5ef1b2c735b2fc2afdfed210a3a5aa01c48e572326914e27bb221",
    ),
    sdl2_Windows=dict(
        version="2.26.5",
        url="https://github.com/libsdl-org/SDL/releases/download/release-@VERSION@/SDL2-devel-@VERSION@-VC.zip",
        dst_file="SDL2-@VERSION@.zip",
        sha256="446cf6277ff0dd4211e6dc19c1b9015210a72758f53f5034c7e4d6b60e540ecf",
    ),
    glslang_Windows=dict(
        # Use the legacy master-tot Windows build until the main-tot one is
        # fixed, or until the glslang project provides Windows builds for their
        # stable releases
        # See: https://github.com/KhronosGroup/glslang/issues/3186
        version="master",
        dst_file="glslang-@VERSION@.zip",
        dst_dir="glslang-@VERSION@",
        url="https://github.com/KhronosGroup/glslang/releases/download/master-tot/glslang-master-windows-x64-Release.zip",
        sha256="skip",
    ),
    pkgconf=dict(
        version="1.9.5",
        url="https://github.com/pkgconf/pkgconf/archive/refs/tags/pkgconf-@VERSION@.tar.gz",
        sha256="3fd0ace7d4398d75f0c566759f9beb4f8b3984fee7ed9804d41c52b193d9745a",
    ),
    renderdoc_Windows=dict(
        version="1.29",
        url="https://renderdoc.org/stable/@VERSION@/RenderDoc_@VERSION@_64.zip",
        sha256="6307cb8e342237f4c1814a9203428606db5b33722cb7ec0d17c3b930c2ee4584",
    ),
    renderdoc_Linux=dict(
        version="1.29",
        url="https://renderdoc.org/stable/@VERSION@/renderdoc_@VERSION@.tar.gz",
        sha256="ae61cd6cfd82930c5928aed8a6d2c7f4a6b13df54033975425386a19efdbf0e7",
    ),
    freetype=dict(
        version="2-13-2",
        url="https://github.com/freetype/freetype/archive/refs/tags/VER-@VERSION@.tar.gz",
        sha256="427201f5d5151670d05c1f5b45bef5dda1f2e7dd971ef54f0feaaa7ffd2ab90c",
    ),
    harfbuzz=dict(
        version="8.1.1",
        url="https://github.com/harfbuzz/harfbuzz/archive/refs/tags/@VERSION@.tar.gz",
        sha256="b16e6bc0fc7e6a218583f40c7d201771f2e3072f85ef6e9217b36c1dc6b2aa25",
    ),
    fribidi=dict(
        version="1.0.13",
        url="https://github.com/fribidi/fribidi/archive/refs/tags/v@VERSION@.tar.gz",
        sha256="f24e8e381bcf76533ae56bd776196f3a0369ec28e9c0fdb6edd163277e008314",
    ),
)


def _get_external_deps(args):
    deps = ["nopemd"]
    if _SYSTEM == "Windows":
        deps.append("pkgconf")
        deps.append("egl_registry")
        deps.append("opengl_registry")
        deps.append("ffmpeg_Windows")
        deps.append("sdl2_Windows")
        deps.append("glslang_Windows")
        deps.append("freetype")
        deps.append("harfbuzz")
        deps.append("fribidi")
    if "gpu_capture" in args.debug_opts:
        if _SYSTEM not in {"Windows", "Linux"}:
            raise Exception(f"Renderdoc is not supported on {_SYSTEM}")
        deps.append(_RENDERDOC_ID)
    return {dep: _EXTERNAL_DEPS[dep] for dep in deps}


def _guess_base_dir(dirs):
    smallest_dir = sorted(dirs, key=lambda x: len(x))[0]
    return pathlib.Path(smallest_dir).parts[0]


def _get_brew_prefix():
    prefix = None
    try:
        proc = run(["brew", "--prefix"], capture_output=True, text=True, check=True)
        prefix = proc.stdout.strip()
    except FileNotFoundError:
        # Silently pass if brew is not installed
        pass
    return prefix


def _file_chk(path, chksum_hexdigest):
    if chksum_hexdigest == "skip":
        return True
    chksum = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            buf = f.read(8196)
            if not buf:
                break
            chksum.update(buf)
    match_ = chksum.hexdigest() == chksum_hexdigest
    if not match_:
        logging.warning("%s: mismatching check sum", path)
    return match_


def _fix_permissions(path):
    for root, dirs, files in os.walk(path, topdown=True):
        for file in files:
            os.chmod(op.join(root, file), stat.S_IRUSR | stat.S_IWUSR)
        for directory in dirs:
            os.chmod(op.join(root, directory), stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR)


def _rmtree(path, ignore_errors=False, onerror=None):
    """
    shutil.rmtree wrapper that is resilient to permission issues when
    encountering read-only files or directories lacking the executable
    permission.
    """
    try:
        shutil.rmtree(path, ignore_errors, onerror=onerror)
    except Exception:
        _fix_permissions(path)
        shutil.rmtree(path, ignore_errors, onerror=onerror)


def _download_extract(dep_item):
    logging.basicConfig(level="INFO")  # Needed for every process on Windows

    name, dep = dep_item

    version = dep["version"]
    url = dep["url"].replace("@VERSION@", version)
    chksum = dep["sha256"]
    dst_file = dep.get("dst_file", op.basename(url)).replace("@VERSION@", version)
    dst_dir = dep.get("dst_dir", "").replace("@VERSION@", version)
    dst_base = op.join(_ROOTDIR, "external")
    dst_path = op.join(dst_base, dst_file)
    os.makedirs(dst_base, exist_ok=True)

    # Download
    if not op.exists(dst_path) or not _file_chk(dst_path, chksum):
        logging.info("downloading %s to %s", url, dst_file)
        urllib.request.urlretrieve(url, dst_path)
        assert _file_chk(dst_path, chksum)

    # Extract
    if tarfile.is_tarfile(dst_path):
        with tarfile.open(dst_path) as tar:
            dirs = {f.name for f in tar.getmembers() if f.isdir()}
            extract_dir = op.join(dst_base, dst_dir or _guess_base_dir(dirs))
            if not op.exists(extract_dir):
                logging.info("extracting %s", dst_file)
                tar.extractall(op.join(dst_base, dst_dir))

    elif zipfile.is_zipfile(dst_path):
        with zipfile.ZipFile(dst_path) as zip_:
            dirs = {op.dirname(f) for f in zip_.namelist()}
            extract_dir = op.join(dst_base, dst_dir or _guess_base_dir(dirs))
            if not op.exists(extract_dir):
                logging.info("extracting %s", dst_file)
                zip_.extractall(op.join(dst_base, dst_dir))
    else:
        assert False

    # Remove previous link if needed
    target = op.join(dst_base, name)
    rel_extract_dir = op.basename(extract_dir)
    if op.islink(target) and os.readlink(target) != rel_extract_dir:
        logging.info("unlink %s target", target)
        os.unlink(target)
    elif op.exists(target) and not op.islink(target):
        logging.info("remove previous %s copy", target)
        _rmtree(target)

    # Link (or copy)
    if not op.exists(target):
        logging.info("symlink %s -> %s", target, rel_extract_dir)
        try:
            os.symlink(rel_extract_dir, target)
        except OSError:
            # This typically happens on Windows when Developer Mode is not
            # available/enabled
            logging.info("unable to symlink, fallback on copy (%s -> %s)", extract_dir, target)
            shutil.copytree(extract_dir, target)

    return name, target


def _fetch_externals(args):
    dependencies = _get_external_deps(args)
    with Pool() as p:
        return dict(p.map(_download_extract, dependencies.items()))


def _block(name, prerequisites=None):
    def real_decorator(block_func):
        block_func.name = name
        block_func.prerequisites = prerequisites if prerequisites else []
        return block_func

    return real_decorator


def _meson_compile_install_cmd(cfg, component, external=False):
    builddir = op.join(cfg.externals[component], "builddir") if external else op.join("builddir", component)
    return ["$(MESON) " + _cmd_join(action, "-C", builddir) for action in ("compile", "install")]


@_block("pkgconf-setup")
def _pkgconf_setup(cfg):
    builddir = op.join("external", "pkgconf", "builddir")
    return ["$(MESON_SETUP) " + _cmd_join("-Dtests=disabled", cfg.externals["pkgconf"], builddir)]


@_block("pkgconf-install", [_pkgconf_setup])
def _pkgconf_install(cfg):
    ret = _meson_compile_install_cmd(cfg, "pkgconf", external=True)
    pkgconf_exe = op.join(cfg.bin_path, "pkgconf.exe")
    pkgconfig_exe = op.join(cfg.bin_path, "pkg-config.exe")
    return ret + [f"copy {pkgconf_exe} {pkgconfig_exe}"]


@_block("egl-registry-install", [])
def _egl_registry_install(cfg):
    dirs = (
        "EGL",
        "KHR",
    )
    cmds = []
    for d in dirs:
        src = op.join(cfg.externals["egl_registry"], "api", d)
        dst = op.join(cfg.prefix, "Include", d)
        cmds.append(_cmd_join("xcopy", src, dst, "/s", "/y", "/i"))
    return cmds


@_block("opengl-registry-install", [])
def _opengl_registry_install(cfg):
    dirs = (
        "GL",
        "GLES",
        "GLES2",
        "GLES3",
        "GLSC",
        "GLSC2",
    )
    cmds = []
    for d in dirs:
        src = op.join(cfg.externals["opengl_registry"], "api", d)
        dst = op.join(cfg.prefix, "Include", d)
        cmds.append(_cmd_join("xcopy", src, dst, "/s", "/y", "/i"))
    return cmds


@_block("ffmpeg-install", [])
def _ffmpeg_install(cfg):
    dirs = (
        ("bin", "Scripts"),
        ("lib", "Lib"),
        ("include", "Include"),
    )
    cmds = []
    for src, dst in dirs:
        src = op.join(cfg.externals["ffmpeg_Windows"], src, "*")
        dst = op.join(cfg.prefix, dst)
        cmds.append(_cmd_join("xcopy", src, dst, "/s", "/y"))
    return cmds


@_block("sdl2-install", [])
def _sdl2_install(cfg):
    dirs = (
        "lib",
        "include",
        "cmake",
    )
    cmds = []
    for d in dirs:
        src = op.join(cfg.externals["sdl2_Windows"], d, "*")
        dst = op.join(cfg.prefix, d)
        os.makedirs(dst, exist_ok=True)
        cmds.append(_cmd_join("xcopy", src, dst, "/s", "/y"))

    src = op.join(cfg.externals["sdl2_Windows"], "lib", "x64", "SDL2.dll")
    dst = op.join(cfg.prefix, "Scripts")
    cmds.append(_cmd_join("xcopy", src, dst, "/y"))

    return cmds


@_block("glslang-install", [])
def _glslang_install(cfg):
    dirs = (
        ("lib", "Lib"),
        ("include", "Include"),
        ("bin", "Scripts"),
    )
    cmds = []
    for src, dst in dirs:
        src = op.join(cfg.externals["glslang_Windows"], src, "*")
        dst = op.join(cfg.prefix, dst)
        cmds.append(_cmd_join("xcopy", src, dst, "/s", "/y"))

    return cmds


@_block(
    "nopemd-setup",
    {
        "Windows": [_pkgconf_install, _ffmpeg_install, _sdl2_install],
    },
)
def _nopemd_setup(cfg):
    builddir = op.join("external", "nopemd", "builddir")
    return ["$(MESON_SETUP) -Drpath=true " + _cmd_join(cfg.externals["nopemd"], builddir)]


@_block("nopemd-install", [_nopemd_setup])
def _nopemd_install(cfg):
    return _meson_compile_install_cmd(cfg, "nopemd", external=True)


@_block("renderdoc-install")
def _renderdoc_install(cfg):
    renderdoc_dll = op.join(cfg.externals[_RENDERDOC_ID], "renderdoc.dll")
    return [f"copy {renderdoc_dll} {cfg.bin_path}"]


@_block("freetype-setup", {"Windows": [_pkgconf_install]})
def _freetype_setup(cfg):
    builddir = op.join("external", "freetype", "builddir")
    return ["$(MESON_SETUP) " + _cmd_join(cfg.externals["freetype"], builddir)]


@_block("freetype-install", [_freetype_setup])
def _freetype_install(cfg):
    return _meson_compile_install_cmd(cfg, "freetype", external=True)


@_block("harfbuzz-setup", [_freetype_install])
def _harfbuzz_setup(cfg):
    builddir = op.join("external", "harfbuzz", "builddir")
    return ["$(MESON_SETUP) " + _cmd_join(cfg.externals["harfbuzz"], builddir)]


@_block("harfbuzz-install", [_harfbuzz_setup])
def _harfbuzz_install(cfg):
    return _meson_compile_install_cmd(cfg, "harfbuzz", external=True)


@_block("fribidi-setup", {"Windows": [_pkgconf_install]})
def _fribidi_setup(cfg):
    builddir = op.join("external", "fribidi", "builddir")
    return ["$(MESON_SETUP) " + _cmd_join("-Ddocs=false", cfg.externals["fribidi"], builddir)]


@_block("fribidi-install", [_fribidi_setup])
def _fribidi_install(cfg):
    return _meson_compile_install_cmd(cfg, "fribidi", external=True)


@_block(
    "nopegl-setup",
    {
        "Linux": [_nopemd_install],
        "Darwin": [_nopemd_install],
        "MinGW": [_nopemd_install],
        "Windows": [
            _nopemd_install,
            _sdl2_install,
            _egl_registry_install,
            _opengl_registry_install,
            _glslang_install,
            _freetype_install,
            _harfbuzz_install,
            _fribidi_install,
        ],
    },
)
def _nopegl_setup(cfg):
    nopegl_opts = []
    if cfg.args.debug_opts:
        debug_opts = ",".join(cfg.args.debug_opts)
        nopegl_opts += [f"-Ddebug_opts={debug_opts}"]

    if cfg.args.sanitize:
        nopegl_opts += [f"-Db_sanitize={cfg.args.sanitize}"]

    if "gpu_capture" in cfg.args.debug_opts:
        renderdoc_dir = cfg.externals[_RENDERDOC_ID]
        nopegl_opts += [f"-Drenderdoc_dir={renderdoc_dir}"]

    extra_library_dirs = []
    extra_include_dirs = []
    if _SYSTEM == "Windows":
        extra_library_dirs += [op.join(cfg.prefix, "Lib")]
        extra_include_dirs += [op.join(cfg.prefix, "Include")]

    elif _SYSTEM == "Darwin":
        prefix = _get_brew_prefix()
        if prefix:
            extra_library_dirs += [op.join(prefix, "lib")]
            extra_include_dirs += [op.join(prefix, "include")]

    if extra_library_dirs:
        opts = ",".join(extra_library_dirs)
        nopegl_opts += [f"-Dextra_library_dirs={opts}"]

    if extra_include_dirs:
        opts = ",".join(extra_include_dirs)
        nopegl_opts += [f"-Dextra_include_dirs={opts}"]

    return ["$(MESON_SETUP) -Drpath=true " + _cmd_join(*nopegl_opts, "libnopegl", op.join("builddir", "libnopegl"))]


@_block("nopegl-install", [_nopegl_setup])
def _nopegl_install(cfg):
    return _meson_compile_install_cmd(cfg, "libnopegl")


@_block("nopegl-install-nosetup")
def _nopegl_install_nosetup(cfg):
    return _meson_compile_install_cmd(cfg, "libnopegl")


@_block("pynopegl-deps-install", [_nopegl_install])
def _pynopegl_deps_install(cfg):
    return ["$(PIP) " + _cmd_join("install", "-r", op.join(".", "pynopegl", "requirements.txt"))]


@_block("pynopegl-install", [_pynopegl_deps_install])
def _pynopegl_install(cfg):
    ret = ["$(PIP) " + _cmd_join("-v", "install", "-e", op.join(".", "pynopegl"))]
    if _SYSTEM != "Windows":
        rpath = op.join(cfg.prefix, "lib")
        ldflags = f"-Wl,-rpath,{rpath}"
        ret[0] = f"LDFLAGS={ldflags} {ret[0]}"
    return ret


@_block("pynopegl-utils-deps-install", [_pynopegl_install])
def _pynopegl_utils_deps_install(cfg):
    #
    # Requirements not installed on MinGW because:
    # - PySide6 can't be pulled (required to be installed by the user outside the
    #   Python virtual env)
    # - Pillow fails to find zlib (required to be installed by the user outside the
    #   Python virtual env)
    #
    if _SYSTEM == "MinGW":
        return ["@"]  # noop
    return ["$(PIP) " + _cmd_join("install", "-r", op.join(".", "pynopegl-utils", "requirements.txt"))]


@_block("pynopegl-utils-install", [_pynopegl_utils_deps_install])
def _pynopegl_utils_install(cfg):
    return ["$(PIP) " + _cmd_join("-v", "install", "-e", op.join(".", "pynopegl-utils"))]


@_block("ngl-tools-setup", [_nopegl_install])
def _ngl_tools_setup(cfg):
    return ["$(MESON_SETUP) -Drpath=true " + _cmd_join("ngl-tools", op.join("builddir", "ngl-tools"))]


@_block("ngl-tools-install", [_ngl_tools_setup])
def _ngl_tools_install(cfg):
    return _meson_compile_install_cmd(cfg, "ngl-tools")


@_block("ngl-tools-install-nosetup", [_nopegl_install_nosetup])
def _ngl_tools_install_nosetup(cfg):
    return _meson_compile_install_cmd(cfg, "ngl-tools")


def _nopegl_run_target_cmd(cfg, target):
    builddir = op.join("builddir", "libnopegl")
    return ["$(MESON) " + _cmd_join("compile", "-C", builddir, target)]


@_block("nopegl-updatedoc", [_nopegl_install])
def _nopegl_updatedoc(cfg):
    gen_doc = op.join("scripts", "gen-doc.py")
    specs_path = op.join(cfg.args.venv_path, "share", "nopegl", "nodes.specs")
    output_path = op.join("doc", "usr", "ref", "libnopegl.md")
    return ["$(PYTHON) " + _cmd_join(gen_doc, specs_path, output_path)]


@_block("nopegl-updatespecs", [_nopegl_install])
def _nopegl_updatespecs(cfg):
    return _nopegl_run_target_cmd(cfg, "updatespecs")


@_block("nopegl-updateglwrappers", [_nopegl_install])
def _nopegl_updateglwrappers(cfg):
    return _nopegl_run_target_cmd(cfg, "updateglwrappers")


@_block("all", [_ngl_tools_install, _pynopegl_utils_install])
def _all(cfg):
    echo = ["", "Build completed.", "", "You can now enter the venv with:"]
    if _SYSTEM == "Windows":
        echo.append(op.join(cfg.bin_path, "ngli-activate.ps1"))
        return [f"@echo.{e}" for e in echo]
    else:
        echo.append(" " * 4 + ". " + op.join(cfg.bin_path, "ngli-activate"))
        return [f'@echo "    {e}"' for e in echo]


@_block("tests-setup", [_ngl_tools_install, _pynopegl_utils_install])
def _tests_setup(cfg):
    return ["$(MESON_SETUP_TESTS) " + _cmd_join("tests", op.join("builddir", "tests"))]


@_block("nopegl-tests", [_nopegl_install])
def _nopegl_tests(cfg):
    return ["$(MESON) " + _cmd_join("test", "-C", op.join("builddir", "libnopegl"))]


def _rm(f):
    return f"(if exist {f} del /q {f})" if _SYSTEM == "Windows" else f"$(RM) {f}"


def _rd(d):
    return f"(if exist {d} rd /s /q {d})" if _SYSTEM == "Windows" else f"$(RM) -r {d}"


@_block("clean-py")
def _clean_py(cfg):
    return [
        _rm(op.join("pynopegl", "nodes_def.pyx")),
        _rm(op.join("pynopegl", "_pynopegl.c")),
        _rm(op.join("pynopegl", "_pynopegl.*.so")),
        _rm(op.join("pynopegl", "pynopegl.*.pyd")),
        _rm(op.join("pynopegl", "pynopegl", "__init__.py")),
        _rd(op.join("pynopegl", "build")),
        _rd(op.join("pynopegl", "pynopegl.egg-info")),
        _rd(op.join("pynopegl", ".eggs")),
        _rd(op.join("pynopegl-utils", "pynopegl_utils.egg-info")),
        _rd(op.join("pynopegl-utils", ".eggs")),
    ]


@_block("clean", [_clean_py])
def _clean(cfg):
    return [
        _rd(op.join("builddir", "doc")),
        _rd(op.join("builddir", "libnopegl")),
        _rd(op.join("builddir", "ngl-tools")),
        _rd(op.join("builddir", "tests")),
        _rd(op.join("external", "pkgconf", "builddir")),
        _rd(op.join("external", "nopemd", "builddir")),
        _rd(op.join("external", "freetype", "builddir")),
        _rd(op.join("external", "harfbuzz", "builddir")),
        _rd(op.join("external", "fribidi", "builddir")),
    ]


def _coverage(cfg, output):
    # We don't use `meson coverage` here because of
    # https://github.com/mesonbuild/meson/issues/7895
    return [_cmd_join("ninja", "-C", op.join("builddir", "libnopegl"), f"coverage-{output}")]


@_block("coverage-html")
def _coverage_html(cfg):
    return _coverage(cfg, "html")


@_block("coverage-xml")
def _coverage_xml(cfg):
    return _coverage(cfg, "xml")


@_block("tests", [_nopegl_tests, _tests_setup])
def _tests(cfg):
    return ["$(MESON) " + _cmd_join("test", "-C", op.join("builddir", "tests"))]


@_block("htmldoc-deps-install")
def _htmldoc_deps_install(cfg):
    return ["$(PIP) " + _cmd_join("install", "-r", op.join(".", "doc", "requirements.txt"))]


@_block("htmldoc", [_nopegl_updatedoc, _htmldoc_deps_install])
def _htmldoc(cfg):
    return [_cmd_join("sphinx-build", "-W", "doc", op.join("builddir", "doc"))]


def _quote(s):
    if not s or " " in s:
        return f'"{s}"'
    assert "'" not in s
    assert '"' not in s
    return s


def _cmd_join(*cmds):
    if _SYSTEM == "Windows":
        return " ".join(_quote(cmd) for cmd in cmds)
    return shlex.join(cmds)


def _get_make_vars(cfg):
    debug = cfg.args.coverage or cfg.args.buildtype == "debug"

    # We don't want Python to fallback on one found in the PATH so we explicit
    # it to the one in the venv.
    python = op.join(cfg.bin_path, "python")

    #
    # MAKEFLAGS= is a workaround (not working on Windows due to incompatible Make
    # syntax) for the issue described here:
    # https://github.com/ninja-build/ninja/issues/1139#issuecomment-724061270
    #
    # Note: this will invoke the meson in the venv, unless we're on MinGW where
    # it will fallback on the system one. This is due to the extended PATH
    # mechanism.
    #
    meson = "MAKEFLAGS= meson" if _SYSTEM != "Windows" else "meson"

    meson_setup = [
        "setup",
        "--prefix",
        cfg.prefix,
        "--pkg-config-path",
        cfg.pkg_config_path,
        "--buildtype",
        "debugoptimized" if debug else "release",
    ]
    if cfg.args.coverage:
        meson_setup += ["-Db_coverage=true"]
    if _SYSTEM != "MinGW" and "debug" not in cfg.args.buildtype:
        meson_setup += ["-Db_lto=true"]

    if _SYSTEM == "Windows":
        meson_setup += ["--bindir=Scripts", "--libdir=Lib", "--includedir=Include"]
    elif op.isfile("/etc/debian_version"):
        # Workaround Debian/Ubuntu bug; see https://github.com/mesonbuild/meson/issues/5925
        meson_setup += ["--libdir=lib"]

    ret = dict(
        PYTHON=python,
        PIP=_cmd_join(python, "-m", "pip"),
        MESON=meson,
    )

    ret["MESON_SETUP"] = "$(MESON) " + _cmd_join(*meson_setup, f"--backend={cfg.args.build_backend}")
    # Our tests/meson.build logic is not well supported with the VS backend so
    # we need to fallback on Ninja
    ret["MESON_SETUP_TESTS"] = "$(MESON) " + _cmd_join(*meson_setup, "--backend=ninja")

    return ret


def _add_prerequisite(cfg, block, dep):
    if isinstance(block.prerequisites, list):
        block.prerequisites.append(dep)
    elif isinstance(block.prerequisites, dict):
        if _SYSTEM in block.prerequisites:
            block.prerequisites[_SYSTEM].append(dep)
        else:
            block.prerequisites[_SYSTEM] = [dep]
    else:
        assert block.prerequisites is None


def _get_prerequisites(cfg, block):
    prereqs = []
    if isinstance(block.prerequisites, list):
        prereqs = block.prerequisites
    elif isinstance(block.prerequisites, dict):
        if _SYSTEM in block.prerequisites:
            prereqs = block.prerequisites[_SYSTEM]
    else:
        assert block.prerequisites is None
    return prereqs


def _get_makefile_rec(cfg, blocks, declared):
    ret = ""
    for block in blocks:
        if block.name in declared:
            continue
        declared |= {block.name}
        prereqs = _get_prerequisites(cfg, block)
        req_names = " ".join(r.name for r in prereqs)
        req = f" {req_names}" if req_names else ""
        commands = "\n".join("\t" + cmd for cmd in block(cfg))
        ret += f"{block.name}:{req}\n{commands}\n"
        ret += _get_makefile_rec(cfg, prereqs, declared)
    return ret


def _get_makefile(cfg, blocks):
    env = cfg.get_env()
    env_vars = {k: _quote(v) for k, v in env.items()}
    if _SYSTEM == "Windows":
        #
        # Environment variables are altered if and only if they already exists
        # in the environment. While this is (usually) true for PATH, it isn't
        # for the others we're trying to declare. This "if [set...]" trick is
        # to circumvent this issue.
        #
        # See https://stackoverflow.com/questions/38381422/how-do-i-set-an-environment-variables-with-nmake
        #
        vars_export = "\n".join(f"{k} = {v}" for k, v in env_vars.items()) + "\n"
        vars_export_cond = " || ".join(f"[set {k}=$({k})]" for k in env_vars.keys())
        vars_export += f"!if {vars_export_cond}\n!endif\n"
    else:
        # We must immediate assign with ":=" since sometimes values contain
        # reference to themselves (typically PATH)
        vars_export = "\n".join(f"export {k} := {v}" for k, v in env_vars.items()) + "\n"

    make_vars = _get_make_vars(cfg)
    ret = "\n".join(f"{k} = {v}" for k, v in make_vars.items()) + "\n" + vars_export

    declared = set()
    ret += _get_makefile_rec(cfg, blocks, declared)
    ret += ".PHONY: " + " ".join(declared) + "\n"
    return ret


class _EnvBuilder(venv.EnvBuilder):
    def __init__(self):
        super().__init__(system_site_packages=_SYSTEM == "MinGW", with_pip=True, prompt="nopegl")

    def post_setup(self, context):
        if _SYSTEM == "MinGW":
            return
        pip_install = [context.env_exe, "-m", "pip", "install"]
        pip_install += ["meson", "ninja"]
        logging.info("install build dependencies: %s", _cmd_join(*pip_install))
        run(pip_install, check=True)


def _build_venv(args):
    if op.exists(args.venv_path):
        logging.warning("Python virtual env already exists at %s", args.venv_path)
        return
    logging.info("creating Python virtualenv: %s", args.venv_path)
    _EnvBuilder().create(args.venv_path)


class _Config:
    def __init__(self, args, externals):
        self.args = args
        self.prefix = op.abspath(args.venv_path)
        self.bin_name = "Scripts" if _SYSTEM == "Windows" else "bin"
        self.bin_path = op.join(self.prefix, self.bin_name)
        self.pkg_config_path = op.join(self.prefix, "lib", "pkgconfig")
        self.externals = externals

        if _SYSTEM == "Windows":
            if "gpu_capture" in args.debug_opts:
                _add_prerequisite(self, _nopegl_setup, _renderdoc_install)

    def get_env(self):
        sep = ":" if _SYSTEM == "MinGW" else os.pathsep
        env = {}
        env["PATH"] = sep.join((self.bin_path, "$(PATH)"))
        env["PKG_CONFIG_PATH"] = self.pkg_config_path
        if _SYSTEM == "Windows":
            env["PKG_CONFIG_ALLOW_SYSTEM_LIBS"] = "1"
            env["PKG_CONFIG_ALLOW_SYSTEM_CFLAGS"] = "1"
            env["CMAKE_PREFIX_PATH"] = op.join(self.prefix, "cmake")
        elif _SYSTEM == "MinGW":
            # See https://setuptools.pypa.io/en/latest/deprecated/distutils-legacy.html
            env["SETUPTOOLS_USE_DISTUTILS"] = "stdlib"
        return env


def _build_env_scripts(cfg):
    if _SYSTEM == "Windows":
        activate_path = os.path.join(cfg.bin_path, "Activate.ps1")
        ngl_activate_ps1 = textwrap.dedent(
            f"""\
            function global:ngli_deactivate() {{
                deactivate
                if (Test-Path -Path Env:NGL_DLL_DIRS) {{
                    Remove-Item -Path Env:NGL_DLL_DIRS
                }}
                Remove-Item -Path function:ngli_deactivate
            }}

            . {activate_path}
            $Env:NGL_DLL_DIRS="{cfg.bin_path}"
            """
        )
        with open(op.join(cfg.bin_path, "ngli-activate.ps1"), "w") as fp:
            fp.write(ngl_activate_ps1)

        return

    activate_path = os.path.join(cfg.bin_path, "activate")
    ngl_activate = textwrap.dedent(
        f"""\
        ngli_deactivate() {{
            deactivate
            unset NGL_DLL_DIRS
            unset -f ngli_deactivate
        }}

        . {activate_path}
        export NGL_DLL_DIRS="{cfg.bin_path}"
        """
    )
    with open(op.join(cfg.bin_path, "ngli-activate"), "w") as fp:
        fp.write(ngl_activate)


def _run():
    default_build_backend = "ninja" if _SYSTEM != "Windows" else "vs"
    parser = argparse.ArgumentParser(
        description="Create and manage a standalone nope.gl virtual environement",
    )
    parser.add_argument("-p", "--venv-path", default=op.join(_ROOTDIR, "venv"), help="Virtual environment directory")
    parser.add_argument("--buildtype", choices=("release", "debug"), default="release", help="Build type")
    parser.add_argument("--coverage", action="store_true", help="Code coverage")
    parser.add_argument(
        "--sanitize",
        choices=("none", "address", "thread", "undefined", "memory", "leak", "address,undefined"),
        help="Code sanitizer to use",
    )
    parser.add_argument(
        "-d",
        "--debug-opts",
        nargs="+",
        default=[],
        choices=("gl", "vk", "mem", "scene", "gpu_capture"),
        help="Debug options",
    )
    parser.add_argument(
        "--build-backend", choices=("ninja", "vs"), default=default_build_backend, help="Build backend to use"
    )
    parser.add_argument(
        "--download-only",
        action=argparse.BooleanOptionalAction,
        help="Only download the external dependencies",
    )

    args = parser.parse_args()

    logging.basicConfig(level="INFO")

    externals = _fetch_externals(args)
    if args.download_only:
        return

    _build_venv(args)

    cfg = _Config(args, externals)

    blocks = [
        _all,
        _tests,
        _clean,
        _nopegl_updatedoc,
        _nopegl_updatespecs,
        _nopegl_updateglwrappers,
        _ngl_tools_install_nosetup,
        _htmldoc,
    ]
    if args.coverage:
        blocks += [_coverage_html, _coverage_xml]

    dst_makefile = op.join(_ROOTDIR, "Makefile")
    logging.info("writing %s", dst_makefile)
    makefile = _get_makefile(cfg, blocks)
    with open(dst_makefile, "w") as f:
        f.write(makefile)

    _build_env_scripts(cfg)


if __name__ == "__main__":
    _run()
