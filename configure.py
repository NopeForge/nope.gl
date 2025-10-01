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
import functools
import hashlib
import logging
import os
import os.path as op
import pathlib
import platform
import shlex
import shutil
import stat
import subprocess
import sysconfig
import tarfile
import textwrap
import urllib.request
import venv
import zipfile
from multiprocessing import Pool
from subprocess import run

_CPU_FAMILIES = [
    "arm",
    "aarch64",
    "x86_64",
]

_ANDROID_VERSION = 28

_ANDROID_COMPILER_MAP = dict(
    arm=f"armv7a-linux-androideabi{_ANDROID_VERSION}",
    aarch64=f"aarch64-linux-android{_ANDROID_VERSION}",
    x86_64=f"x86_64-linux-android{_ANDROID_VERSION}",
)

_ANDROID_ABI_MAP = dict(
    arm="armeabi-v7a",
    aarch64="arm64-v8a",
    x86_64="x86_64",
)

_ANDROID_BUILD_MAP = dict(
    Linux="linux-x86_64",
    Darwin="darwin-x86_64",
    Windows="windows-x86_64",
)

_ANDROID_CROSS_FILE_TPL = textwrap.dedent(
    """\
    [constants]
    ndk_home = '{ndk_home}'
    toolchain = ndk_home / 'toolchains/llvm/prebuilt/{ndk_build}/bin'
    [binaries]
    c = toolchain / '{compiler}-clang'
    cpp = toolchain / '{compiler}-clang++'
    strip = toolchain / 'llvm-strip'
    ar = toolchain / 'llvm-ar'
    pkg-config = 'pkg-config'
    [host_machine]
    system = 'android'
    cpu_family = '{cpu_family}'
    cpu = '{cpu}'
    endian = 'little'
    """
)


def _get_android_cross_file_path(cfg):
    return op.join(cfg.prefix, f"meson-android-{cfg.args.host_arch}.ini")


def _gen_android_cross_file(cfg):
    cross_file = _ANDROID_CROSS_FILE_TPL.format(
        ndk_home=cfg.android_ndk_home,
        ndk_build=cfg.android_ndk_build,
        compiler=cfg.android_compiler,
        cpu_family=cfg.cpu_family,
        cpu=cfg.cpu,
    )
    cross_file_path = _get_android_cross_file_path(cfg)
    os.makedirs(op.dirname(cross_file_path), exist_ok=True)
    with open(cross_file_path, "w") as fp:
        fp.write(cross_file)


_IOS_ABI_MAP = dict(
    aarch64="arm64",
    x86_64="x86_64",
)


_IOS_CROSS_FILE_TPL = textwrap.dedent(
    """\
    [constants]
    root = '/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer'
    sysroot = root / 'SDKs/iPhoneOS.sdk'
    [built-in options]
    c_args = ['-arch', '{abi}', '-isysroot', sysroot]
    c_link_args = ['-arch', '{abi}', '-isysroot', sysroot]
    objc_args = ['-arch', '{abi}', '-isysroot', sysroot]
    objc_link_args = ['-arch', '{abi}', '-isysroot', sysroot]
    cpp_args = ['-arch', '{abi}', '-isysroot', sysroot]
    cpp_link_args = ['-arch', '{abi}', '-isysroot', sysroot]
    objcpp_args = ['-arch', '{abi}', '-isysroot', sysroot]
    objcpp_link_args = ['-arch', '{abi}', '-isysroot', sysroot]
    [properties]
    root = root
    [binaries]
    c = 'clang'
    cpp = 'clang++'
    objc = 'clang'
    strip = 'strip'
    pkg-config = 'pkg-config'
    [host_machine]
    system = 'darwin'
    subsystem = 'ios'
    cpu_family = '{cpu_family}'
    cpu = '{cpu}'
    endian = 'little'
    """
)


def _get_ios_cross_file_path(cfg):
    return op.join(cfg.prefix, f"meson-ios-{cfg.args.host_arch}.ini")


def _gen_ios_cross_file(cfg):
    cross_file = _IOS_CROSS_FILE_TPL.format(
        abi=cfg.ios_abi,
        cpu_family=cfg.cpu_family,
        cpu=cfg.cpu,
    )
    cross_file_path = _get_ios_cross_file_path(cfg)
    os.makedirs(op.dirname(cross_file_path), exist_ok=True)
    with open(cross_file_path, "w") as fp:
        fp.write(cross_file)


_ROOTDIR = op.abspath(op.dirname(__file__))
_SYSTEM = "MinGW" if sysconfig.get_platform().startswith("mingw") else platform.system()
_RENDERDOC_ID = f"renderdoc_{_SYSTEM}"
_EXTERNAL_DEPS = dict(
    boringssl=dict(
        version="059585c",
        url="https://codeload.github.com/google/boringssl/zip/@VERSION@",
        dst_file="boringssl-@VERSION@.zip",
        sha256="d7ed9c743c02469869f4bee7cb94641377b640676cb2a8a9a7955f2d07c4d8a6",
    ),
    ffmpeg=dict(
        version="7.1.1",
        url="https://ffmpeg.org/releases/ffmpeg-@VERSION@.tar.xz",
        dst_file="ffmpeg-@VERSION@.tar.xz",
        sha256="733984395e0dbbe5c046abda2dc49a5544e7e0e1e2366bba849222ae9e3a03b1",
    ),
    ffmpeg_Windows=dict(
        version="7.1.1",
        url="https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2025-03-31-12-55/ffmpeg-n@VERSION@-5-g276bd388f3-win64-lgpl-shared-7.1.zip",
        dst_file="ffmpeg-@VERSION@.zip",
        sha256="a5b24188a1a7c3d4a7a1295bd597f35ad5ef7757796e6a8db1cc1b922ff84e16",
    ),
    nopemd=dict(
        version="13.1.0",
        url="https://github.com/NopeForge/nope.media/archive/v@VERSION@.tar.gz",
        dst_file="nope.media-@VERSION@.tar.gz",
        sha256="fb8dd91282f1a60ea1375edfbb5879783a2df1a9a33c2165f59f5ea5b4e0aa27",
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
        version="2.30.6",
        url="https://github.com/libsdl-org/SDL/releases/download/release-@VERSION@/SDL2-devel-@VERSION@-VC.zip",
        dst_file="SDL2-@VERSION@.zip",
        sha256="65cf810ec730a127f271076ec98d4011177f6ed655d4fc8a94c0bb8dad400922",
    ),
    glslang=dict(
        version="16.0.0",
        dst_file="glslang-@VERSION@.tar.gz",
        url="https://github.com/KhronosGroup/glslang/archive/refs/tags/@VERSION@.tar.gz",
        sha256="172385478520335147d3b03a1587424af0935398184095f24beab128a254ecc7",
    ),
    pkgconf=dict(
        version="1.9.5",
        url="https://github.com/pkgconf/pkgconf/archive/refs/tags/pkgconf-@VERSION@.tar.gz",
        sha256="3fd0ace7d4398d75f0c566759f9beb4f8b3984fee7ed9804d41c52b193d9745a",
    ),
    renderdoc_Windows=dict(
        version="1.34",
        url="https://renderdoc.org/stable/@VERSION@/RenderDoc_@VERSION@_64.zip",
        sha256="6d4cbcca8a9fb93aa6994a4561a4384f6572dc78dadae1c775a780d806c0781e",
    ),
    renderdoc_Linux=dict(
        version="1.34",
        url="https://renderdoc.org/stable/@VERSION@/renderdoc_@VERSION@.tar.gz",
        sha256="adab56edb3e4eab568a0333a0f7f53eb997e887f7ed3bd557135f1a78eb9ec21",
    ),
    freetype=dict(
        version="2-13-3",
        url="https://github.com/freetype/freetype/archive/refs/tags/VER-@VERSION@.tar.gz",
        sha256="bc5c898e4756d373e0d991bab053036c5eb2aa7c0d5c67e8662ddc6da40c4103",
    ),
    harfbuzz=dict(
        version="11.4.4",
        url="https://github.com/harfbuzz/harfbuzz/archive/refs/tags/@VERSION@.tar.gz",
        sha256="f3f333dc2fddf1d0f1e2d963094af2daa6a2fdec28d123050fd560545b1afe54",
    ),
    fribidi=dict(
        version="1.0.16",
        url="https://github.com/fribidi/fribidi/archive/refs/tags/v@VERSION@.tar.gz",
        sha256="5a1d187a33daa58fcee2ad77f0eb9d136dd6fa4096239199ba31e850d397e8a8",
    ),
    moltenvk_Darwin=dict(
        version="1.3.0",
        url="https://github.com/KhronosGroup/MoltenVK/releases/download/v@VERSION@/MoltenVK-macos.tar",
        sha256="57b4184ded521b08a63e3642552ebef8b9e98c2f0bcffa49bd93bbe1301c173d",
    ),
    moltenvk_iOS=dict(
        version="1.3.0",
        url="https://github.com/KhronosGroup/MoltenVK/releases/download/v@VERSION@/MoltenVK-ios.tar",
        sha256="8cff295513dea3ec754d3662070404d2db54b66f110d5fda1e51f9ed5023c9e1",
    ),
)


def _is_local(system):
    return system in {"Linux", "MinGW", "Darwin", "Windows"}


def _get_external_deps(args):
    deps = [
        "nopemd",
        "glslang",
    ]

    host, _ = _get_host(args)
    if host == "Android":
        deps.append("boringssl")
        deps.append("ffmpeg")
        deps.append("freetype")
        deps.append("harfbuzz")
        deps.append("fribidi")
    elif host == "Darwin":
        deps.append("moltenvk_Darwin")
    elif host == "iOS":
        deps.append("boringssl")
        deps.append("ffmpeg")
        deps.append("moltenvk_iOS")
        deps.append("freetype")
        deps.append("harfbuzz")
        deps.append("fribidi")
    elif host == "Windows":
        deps.append("pkgconf")
        deps.append("egl_registry")
        deps.append("opengl_registry")
        deps.append("ffmpeg_Windows")
        deps.append("sdl2_Windows")
        deps.append("freetype")
        deps.append("harfbuzz")
        deps.append("fribidi")

    if "gpu_capture" in args.debug_opts:
        if host not in {"Windows", "Linux"}:
            raise Exception(f"Renderdoc is not supported on {host}")
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


def _get_external_dir(args):
    host, host_arch = _get_host(args)
    if _is_local(host):
        return op.join(_ROOTDIR, "external")
    else:
        return op.join(_ROOTDIR, "external", host, host_arch)


def _download_extract(args, dep_item):
    logging.basicConfig(level="INFO")  # Needed for every process on Windows

    name, dep = dep_item

    version = dep["version"]
    url = dep["url"].replace("@VERSION@", version)
    chksum = dep["sha256"]
    dst_file = dep.get("dst_file", op.basename(url)).replace("@VERSION@", version)
    dst_dir = dep.get("dst_dir", "").replace("@VERSION@", version)
    dst_base = _get_external_dir(args)
    dst_path = op.join(dst_base, dst_file)
    os.makedirs(dst_base, exist_ok=True)

    # Download
    if not op.exists(dst_path) or not _file_chk(dst_path, chksum):
        logging.info("downloading %s to %s", url, dst_file)
        urllib.request.urlretrieve(url, dst_path)
        assert _file_chk(dst_path, chksum)

    # Extract
    extracted = False
    if tarfile.is_tarfile(dst_path):
        with tarfile.open(dst_path) as tar:
            dirs = {f.name for f in tar.getmembers() if f.isdir()}
            extract_dir = op.join(dst_base, dst_dir or _guess_base_dir(dirs))
            if not op.exists(extract_dir):
                logging.info("extracting %s", dst_file)
                tar.extractall(op.join(dst_base, dst_dir), filter="data")
                extracted = True

    elif zipfile.is_zipfile(dst_path):
        with zipfile.ZipFile(dst_path) as zip_:
            dirs = {op.dirname(f) for f in zip_.namelist()}
            extract_dir = op.join(dst_base, dst_dir or _guess_base_dir(dirs))
            if not op.exists(extract_dir):
                logging.info("extracting %s", dst_file)
                zip_.extractall(op.join(dst_base, dst_dir))
                extracted = True
    else:
        assert False

    # Patch
    if extracted:
        patch_basedir = op.join(_ROOTDIR, "patches", name)
        for patch in dep.get("patches", []):
            patch_path = op.join(patch_basedir, patch)
            run(["patch", "-p1", "-i", patch_path], cwd=extract_dir)

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


def _get_host(args):
    if args.host:
        return (args.host, args.host_arch)
    else:
        return (_SYSTEM, platform.machine())


def _fetch_externals(args):
    dependencies = _get_external_deps(args)
    with Pool() as p:
        return dict(p.map(functools.partial(_download_extract, args), dependencies.items()))


def _block(name, prerequisites=None):
    def real_decorator(block_func):
        block_func.name = name
        block_func.prerequisites = prerequisites if prerequisites else []
        return block_func

    return real_decorator


def _get_builddir(cfg, component):
    if component in cfg.externals:
        return op.join(cfg.externals[component], "builddir")

    if _is_local(cfg.host):
        return op.join("builddir", component)
    else:
        return op.join("builddir", cfg.host, cfg.host_arch, component)


def _meson_compile_install_cmd(cfg, component):
    builddir = _get_builddir(cfg, component)
    return ["$(MESON) " + _cmd_join(action, "-C", builddir) for action in ("compile", "install")]


@_block("pkgconf-setup")
def _pkgconf_setup(cfg):
    builddir = _get_builddir(cfg, "pkgconf")
    return ["$(MESON_SETUP) " + _cmd_join("-Dtests=disabled", cfg.externals["pkgconf"], builddir)]


@_block("pkgconf-install", [_pkgconf_setup])
def _pkgconf_install(cfg):
    ret = _meson_compile_install_cmd(cfg, "pkgconf")
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


@_block("boringssl-setup", [])
def _boringssl_setup(cfg):
    build_type = "Debug" if cfg.args.buildtype == "debug" else "Release"
    cmake_args = [
        "cmake",
        "-GNinja",
        f"-DCMAKE_BUILD_TYPE={build_type}",
        f"-DCMAKE_INSTALL_PREFIX={cfg.prefix}",
        "-DBUILD_SHARED_LIBS=OFF",
        "-S",
        cfg.externals["boringssl"],
        "-B",
        _get_builddir(cfg, "boringssl"),
    ]
    if cfg.host == "Android":
        cmake_args += [
            f"-DCMAKE_TOOLCHAIN_FILE={cfg.android_cmake_toolchain}",
            "-DANDROID_STL=c++_shared",
            "-DANDROID_TOOLCHAIN=clang",
            f"-DANDROID_PLATFORM=android-{_ANDROID_VERSION}",
            f"-DANDROID_ABI={cfg.android_abi}",
        ]
    elif cfg.host == "iOS":
        cmake_args += [
            f"-DCMAKE_OSX_SYSROOT=iphoneos",
            f"-DCMAKE_OSX_ARCHITECTURES={cfg.ios_abi}",
        ]
    return [
        _cmd_join(*cmake_args),
    ]


@_block("boringssl-install", [_boringssl_setup])
def _boringssl_install(cfg):
    builddir = _get_builddir(cfg, "boringssl")
    cmds = [
        _cmd_join("cmake", "--build", builddir, "--target", "crypto", "--target", "ssl", "--target", "bssl"),
        _cmd_join("cmake", "--install", builddir),
    ]
    return cmds


@_block("ffmpeg-setup", {"Android": [_boringssl_install], "iOS": [_boringssl_install]})
def _ffmpeg_setup(cfg):
    muxers = [
        "gif",
        "image2",
        "ipod",
        "mov",
        "mp4",
    ]
    parsers = [
        "aac",
        "amr",
        "av1",
        "flac",
        "h264",
        "hevc",
        "mjpeg",
        "png",
        "vp8",
        "vp9",
    ]
    bsfs = [
        "aac_adtstoasc",
        "extract_extradata",
        "h264_mp4toannexb",
        "hevc_mp4toannexb",
        "vp9_superframe",
    ]
    protocols = [
        "fd",
        "file",
        "http",
        "https",
        "pipe",
    ]
    filters = [
        "aformat",
        "aresample",
        "asetnsamples",
        "asettb",
        "copy",
        "dynaudnorm",
        "format",
        "fps",
        "hflip",
        "loudnorm",
        "palettegen",
        "paletteuse",
        "scale",
        "settb",
        "transpose",
        "vflip",
    ]
    demuxers = [
        "aac",
        "aiff",
        "amr",
        "avi",
        "flac",
        "gif",
        "image2",
        "image_jpeg_pipe",
        "image_pgm_pipe",
        "image_png_pipe",
        "image_webp_pipe",
        "matroska",
        "mov",
        "mp3",
        "mp4",
        "mpegts",
        "ogg",
        "rawvideo",
        "wav",
    ]
    decoders = [
        "alac",
        "flac",
        "gif",
        "mjpeg",
        "mp3",
        "opus",
        "pcm_s16be",
        "pcm_s16le",
        "pcm_s24be",
        "pcm_s24le",
        "png",
        "rawvideo",
        "vorbis",
        "vp8",
        "vp9",
        "webp",
    ]
    encoders = ["mjpeg", "png"]
    builddir = _get_builddir(cfg, "ffmpeg")
    os.makedirs(builddir, exist_ok=True)
    extra_include_dir = op.join(cfg.prefix, "include")
    extra_library_dir = op.join(cfg.prefix, "lib")
    extra_cflags = ""
    extra_ldflags = ""
    extra_libs = "-lstdc++"  # Required by BoringSSL
    extra_args = []
    if cfg.host == "Android":
        decoders += [
            "aac_mediacodec",
            "amrnb_mediacodec",
            "amrwb_mediacodec",
            "av1_mediacodec",
            "h264_mediacodec",
            "hevc_mediacodec",
            "vp8_mediacodec",
            "vp9_mediacodec",
        ]
        protocols += ["android_content"]
        extra_args += [
            "--enable-jni",
            "--enable-mediacodec",
            "--target-os=android",
            f"--cross-prefix={cfg.android_ndk_bin}{op.sep}llvm-",
            f"--cc={cfg.android_ndk_bin}{os.sep}{cfg.android_compiler}-clang",
        ]
    elif cfg.host == "iOS":
        decoders += [
            "aac_at",
        ]
        extra_cflags += f"-arch {cfg.ios_abi} -mios-version-min={cfg.ios_version}"
        extra_ldflags += f"-arch {cfg.ios_abi} -mios-version-min={cfg.ios_version}"
        extra_args += [
            "--enable-videotoolbox",
            "--target-os=darwin",
            f"--sysroot={cfg.ios_sdk}",
        ]

    return [
        f"cd {builddir} && "
        + _cmd_join(
            op.join(cfg.externals["ffmpeg"], "configure"),
            "--disable-everything",
            "--disable-doc",
            "--disable-static",
            "--disable-autodetect",
            "--disable-programs",
            "--enable-shared",
            "--enable-cross-compile",
            "--enable-hwaccels",
            "--enable-avdevice",
            "--enable-swresample",
            "--enable-zlib",
            "--enable-openssl",
            "--enable-filter=%s" % ",".join(filters),
            "--enable-bsf=%s" % ",".join(bsfs),
            "--enable-encoder=%s" % ",".join(encoders),
            "--enable-demuxer=%s" % ",".join(demuxers),
            "--enable-decoder=%s" % ",".join(decoders),
            "--enable-parser=%s" % ",".join(parsers),
            "--enable-muxer=%s" % ",".join(muxers),
            "--enable-protocol=%s" % ",".join(protocols),
            f"--arch={cfg.host_arch}",
            f"--extra-cflags=-I{extra_include_dir} {extra_cflags}",
            f"--extra-ldflags=-L{extra_library_dir} {extra_ldflags}",
            f"--extra-libs={extra_libs}",
            f"--prefix={cfg.prefix}",
            *extra_args,
        ),
    ]


@_block("ffmpeg-install", {"Android": [_ffmpeg_setup], "iOS": [_ffmpeg_setup]})
def _ffmpeg_install(cfg):
    if cfg.host == "Windows":
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
    elif cfg.host in ["Android", "iOS"]:
        builddir = _get_builddir(cfg, "ffmpeg")
        cmds = [
            f"cd {builddir} && " + _cmd_join("make", f"-j{os.cpu_count()}"),
            f"cd {builddir} && " + _cmd_join("make", "install"),
        ]
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


@_block("moltenvk-install", [])
def _moltenvk_install(cfg):
    dep_id = f"moltenvk_{cfg.host}"
    if cfg.host == "Darwin":
        arch = "macos-arm64_x86_64"
    elif cfg.host == "iOS":
        arch = "ios-arm64"
    else:
        assert False
    resources = (
        (op.join("static", "MoltenVK.xcframework", arch, "libMoltenVK.a"), "lib"),
        (op.join("include", "."), "include"),
    )
    cmds = []
    for src_path, dst_path in resources:
        src = op.join(cfg.externals[dep_id], "MoltenVK", src_path)
        dst = op.join(cfg.prefix, dst_path)
        os.makedirs(dst, exist_ok=True)
        cmds.append(_cmd_join("cp", "-R", src, dst))
    return cmds


@_block("glslang-setup", [])
def _glslang_setup(cfg):
    build_type = "Debug" if cfg.args.buildtype == "debug" else "Release"
    cmake_args = [
        "cmake",
        "-GNinja",
        f"-DCMAKE_BUILD_TYPE={build_type}",
        f"-DCMAKE_INSTALL_PREFIX={cfg.prefix}",
        "-DBUILD_SHARED_LIBS=OFF",
        "-DBUILD_EXTERNAL=OFF",
        "-DENABLE_OPT=OFF",
        "-S",
        cfg.externals["glslang"],
        "-B",
        _get_builddir(cfg, "glslang"),
    ]
    if cfg.host == "Android":
        cmake_args += [
            f"-DCMAKE_TOOLCHAIN_FILE={cfg.android_cmake_toolchain}",
            "-DANDROID_STL=c++_shared",
            "-DANDROID_TOOLCHAIN=clang",
            f"-DANDROID_PLATFORM=android-{_ANDROID_VERSION}",
            f"-DANDROID_ABI={cfg.android_abi}",
        ]
    elif cfg.host == "iOS":
        cmake_args += [
            f"-DCMAKE_OSX_SYSROOT=iphoneos",
            f"-DCMAKE_OSX_ARCHITECTURES={cfg.ios_abi}",
        ]
    return [
        _cmd_join(*cmake_args),
    ]


@_block("glslang-install", [_glslang_setup])
def _glslang_install(cfg):
    builddir = _get_builddir(cfg, "glslang")
    cmds = [
        _cmd_join("cmake", "--build", builddir),
        _cmd_join("cmake", "--install", builddir),
    ]
    return cmds


@_block(
    "nopemd-setup",
    {
        "Android": [_ffmpeg_install],
        "iOS": [_ffmpeg_install],
        "Windows": [_pkgconf_install, _ffmpeg_install, _sdl2_install],
    },
)
def _nopemd_setup(cfg):
    builddir = _get_builddir(cfg, "nopemd")
    return ["$(MESON_SETUP) -Drpath=true " + _cmd_join(cfg.externals["nopemd"], builddir)]


@_block("nopemd-install", [_nopemd_setup])
def _nopemd_install(cfg):
    return _meson_compile_install_cmd(cfg, "nopemd")


@_block("renderdoc-install")
def _renderdoc_install(cfg):
    renderdoc_dll = op.join(cfg.externals[_RENDERDOC_ID], "renderdoc.dll")
    return [f"copy {renderdoc_dll} {cfg.bin_path}"]


@_block("freetype-setup", {"Windows": [_pkgconf_install]})
def _freetype_setup(cfg):
    builddir = _get_builddir(cfg, "freetype")
    return ["$(MESON_SETUP) " + _cmd_join(cfg.externals["freetype"], builddir)]


@_block("freetype-install", [_freetype_setup])
def _freetype_install(cfg):
    return _meson_compile_install_cmd(cfg, "freetype")


@_block("harfbuzz-setup", [_freetype_install])
def _harfbuzz_setup(cfg):
    builddir = _get_builddir(cfg, "harfbuzz")
    return ["$(MESON_SETUP) -Dtests=disabled " + _cmd_join(cfg.externals["harfbuzz"], builddir)]


@_block("harfbuzz-install", [_harfbuzz_setup])
def _harfbuzz_install(cfg):
    return _meson_compile_install_cmd(cfg, "harfbuzz")


@_block("fribidi-setup", {"Windows": [_pkgconf_install]})
def _fribidi_setup(cfg):
    builddir = _get_builddir(cfg, "fribidi")
    return ["$(MESON_SETUP) " + _cmd_join("-Ddocs=false", cfg.externals["fribidi"], builddir)]


@_block("fribidi-install", [_fribidi_setup])
def _fribidi_install(cfg):
    return _meson_compile_install_cmd(cfg, "fribidi")


@_block(
    "nopegl-setup",
    {
        "Android": [
            _nopemd_install,
            _glslang_install,
            _freetype_install,
            _harfbuzz_install,
            _fribidi_install,
        ],
        "Darwin": [
            _nopemd_install,
            _glslang_install,
            _moltenvk_install,
        ],
        "iOS": [
            _nopemd_install,
            _moltenvk_install,
            _glslang_install,
            _freetype_install,
            _harfbuzz_install,
            _fribidi_install,
        ],
        "Local": [_nopemd_install, _glslang_install],
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
    if cfg.host == "Windows":
        extra_library_dirs += [op.join(cfg.prefix, "Lib")]
        extra_include_dirs += [op.join(cfg.prefix, "Include")]
    else:
        extra_library_dirs += [op.join(cfg.prefix, "lib")]
        extra_include_dirs += [op.join(cfg.prefix, "include")]

    if cfg.host == "Darwin":
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

    return ["$(MESON_SETUP) -Drpath=true " + _cmd_join(*nopegl_opts, "libnopegl", _get_builddir(cfg, "libnopegl"))]


@_block("nopegl-install", [_nopegl_setup])
def _nopegl_install(cfg):
    return _meson_compile_install_cmd(cfg, "libnopegl")


@_block("nopegl-install-nosetup")
def _nopegl_install_nosetup(cfg):
    return _meson_compile_install_cmd(cfg, "libnopegl")


@_block("pynopegl-deps-install", [_nopegl_install])
def _pynopegl_deps_install(cfg):
    return ["$(PIP) " + _cmd_join("install", "-r", op.join("python", "pynopegl", "requirements.txt"))]


@_block("pynopegl-install", [_pynopegl_deps_install])
def _pynopegl_install(cfg):
    ret = ["$(PIP) " + _cmd_join("-v", "install", "-e", op.join("python", "pynopegl"))]
    if cfg.host != "Windows":
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
    if cfg.host == "MinGW":
        return ["@"]  # noop
    return ["$(PIP) " + _cmd_join("install", "-r", op.join("python", "pynopegl-utils", "requirements.txt"))]


@_block("pynopegl-utils-install", [_pynopegl_utils_deps_install])
def _pynopegl_utils_install(cfg):
    return ["$(PIP) " + _cmd_join("-v", "install", "-e", op.join("python", "pynopegl-utils"))]


@_block("ngl-tools-setup", [_nopegl_install])
def _ngl_tools_setup(cfg):
    return ["$(MESON_SETUP) -Drpath=true " + _cmd_join("ngl-tools", _get_builddir(cfg, "ngl-tools"))]


@_block("ngl-tools-install", [_ngl_tools_setup])
def _ngl_tools_install(cfg):
    return _meson_compile_install_cmd(cfg, "ngl-tools")


@_block("ngl-tools-install-nosetup", [_nopegl_install_nosetup])
def _ngl_tools_install_nosetup(cfg):
    return _meson_compile_install_cmd(cfg, "ngl-tools")


def _nopegl_run_target_cmd(cfg, target):
    return ["$(MESON) " + _cmd_join("compile", "-C", _get_builddir(cfg, "libnopegl"), target)]


@_block("nopegl-updatedoc", [_nopegl_install])
def _nopegl_updatedoc(cfg):
    gen_doc = op.join("scripts", "gen-doc.py")
    specs_path = op.join(cfg.venv_path, "share", "nopegl", "nodes.specs")
    output_path = op.join("doc", "usr", "ref", "libnopegl.md")
    return ["$(PYTHON) " + _cmd_join(gen_doc, specs_path, output_path)]


@_block("nopegl-updatespecs", [_nopegl_install])
def _nopegl_updatespecs(cfg):
    return _nopegl_run_target_cmd(cfg, "updatespecs")


@_block("nopegl-updateglwrappers", [_nopegl_install])
def _nopegl_updateglwrappers(cfg):
    return _nopegl_run_target_cmd(cfg, "updateglwrappers")


@_block(
    "all",
    {
        "Local": [_ngl_tools_install, _pynopegl_utils_install],
        "Android": [_nopegl_install],
        "iOS": [_nopegl_install],
    },
)
def _all(cfg):
    echo = ["", "Build completed.", "", "You can now enter the venv with:"]
    if cfg.host == "Windows":
        echo.append(op.join(cfg.venv_bin_path, "ngli-activate.ps1"))
        return [f"@echo.{e}" for e in echo]
    else:
        echo.append(" " * 4 + ". " + op.join(cfg.venv_bin_path, "ngli-activate"))
        return [f'@echo "    {e}"' for e in echo]


@_block("tests-setup", [_ngl_tools_install, _pynopegl_utils_install])
def _tests_setup(cfg):
    return ["$(MESON) setup --backend=ninja " + _cmd_join("tests", _get_builddir(cfg, "tests"))]


@_block("nopegl-tests", [_nopegl_install])
def _nopegl_tests(cfg):
    return ["$(MESON) " + _cmd_join("test", "-C", _get_builddir(cfg, "libnopegl"))]


def _rm(f):
    return f"(if exist {f} del /q {f})" if _SYSTEM == "Windows" else f"$(RM) {f}"


def _rd(d):
    return f"(if exist {d} rd /s /q {d})" if _SYSTEM == "Windows" else f"$(RM) -r {d}"


@_block("clean-py")
def _clean_py(cfg):
    return [
        _rm(op.join("python", "pynopegl", "nodes_def.pyx")),
        _rm(op.join("python", "pynopegl", "_pynopegl.c")),
        _rm(op.join("python", "pynopegl", "_pynopegl.*.so")),
        _rm(op.join("python", "pynopegl", "pynopegl.*.pyd")),
        _rm(op.join("python", "pynopegl", "pynopegl", "__init__.py")),
        _rd(op.join("python", "pynopegl", "build")),
        _rd(op.join("python", "pynopegl", "pynopegl.egg-info")),
        _rd(op.join("python", "pynopegl", ".eggs")),
        _rd(op.join("python", "pynopegl-utils", "pynopegl_utils.egg-info")),
        _rd(op.join("python", "pynopegl-utils", ".eggs")),
    ]


@_block("clean", [_clean_py])
def _clean(cfg):
    deps = ["doc", "libnopegl", "ngl-tools", "tests"] + list(cfg.externals.keys())
    return [_rd(_get_builddir(cfg, dep)) for dep in deps]


def _coverage(cfg, output):
    # We don't use `meson coverage` here because of
    # https://github.com/mesonbuild/meson/issues/7895
    return [_cmd_join("ninja", "-C", _get_builddir(cfg, "libnopegl"), f"coverage-{output}")]


@_block("coverage-html")
def _coverage_html(cfg):
    return _coverage(cfg, "html")


@_block("coverage-xml")
def _coverage_xml(cfg):
    return _coverage(cfg, "xml")


@_block("tests", [_nopegl_tests, _tests_setup])
def _tests(cfg):
    return ["$(MESON) " + _cmd_join("test", "-C", _get_builddir(cfg, "tests"))]


@_block("htmldoc-deps-install")
def _htmldoc_deps_install(cfg):
    return ["$(PIP) " + _cmd_join("install", "-r", op.join(".", "doc", "requirements.txt"))]


@_block("htmldoc", [_nopegl_updatedoc, _htmldoc_deps_install])
def _htmldoc(cfg):
    return [_cmd_join("sphinx-build", "-W", "doc", _get_builddir(cfg, "doc"))]


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
    python = op.join(cfg.venv_bin_path, "python")

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
    if cfg.host != "MinGW" and "debug" not in cfg.args.buildtype:
        meson_setup += ["-Db_lto=true"]

    if cfg.host == "Windows":
        meson_setup += ["--bindir=Scripts", "--libdir=Lib", "--includedir=Include"]
    elif op.isfile("/etc/debian_version"):
        # Workaround Debian/Ubuntu bug; see https://github.com/mesonbuild/meson/issues/5925
        meson_setup += ["--libdir=lib"]

    if cfg.host == "Android":
        meson_setup += ["--cross-file=" + _get_android_cross_file_path(cfg)]
    elif cfg.host == "iOS":
        meson_setup += ["--cross-file=" + _get_ios_cross_file_path(cfg)]

    ret = dict(
        PYTHON=python,
        PIP=_cmd_join(python, "-m", "pip"),
        MESON=meson,
    )

    ret["MESON_SETUP"] = "$(MESON) " + _cmd_join(*meson_setup, f"--backend={cfg.args.build_backend}")

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
        if cfg.host in block.prerequisites:
            prereqs = block.prerequisites[cfg.host]
        elif _is_local(cfg.host) and "Local" in block.prerequisites:
            prereqs = block.prerequisites["Local"]
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
        pip_install += ["meson==1.9.1", "ninja"]
        logging.info("install build dependencies: %s", _cmd_join(*pip_install))
        run(pip_install, check=True)


def _build_venv(args):
    if op.exists(args.venv_path):
        logging.warning("Python virtual env already exists at %s", args.venv_path)
        return
    logging.info("creating Python virtualenv: %s", args.venv_path)
    _EnvBuilder().create(args.venv_path)


def _get_bin_dir(system):
    return "Scripts" if system == "Windows" else "bin"


class _Config:
    def __init__(self, args, externals):
        self.args = args
        self.host, self.host_arch = _get_host(args)

        self.venv_path = op.abspath(args.venv_path)
        self.venv_bin_path = op.join(self.venv_path, _get_bin_dir(_SYSTEM))

        if _is_local(self.host):
            self.prefix = self.venv_path
        elif self.host == "Android":
            self.android_abi = _ANDROID_ABI_MAP[args.host_arch]
            self.prefix = op.join(op.abspath(self.host), self.android_abi)
        elif self.host == "iOS":
            self.ios_abi = _IOS_ABI_MAP[args.host_arch]
            self.ios_platform = "iphoneos"
            self.ios_version = subprocess.run(
                ["xcrun", "--sdk", f"{self.ios_platform}", "--show-sdk-version"],
                capture_output=True,
                text=True,
                check=True,
            ).stdout.strip()
            self.ios_sdk = subprocess.run(
                ["xcrun", "--sdk", f"{self.ios_platform}", "--show-sdk-path"],
                capture_output=True,
                text=True,
                check=True,
            ).stdout.strip()
            self.prefix = op.join(op.abspath(self.host), self.host_arch)
        else:
            assert False

        self.bin_path = op.join(self.prefix, _get_bin_dir(self.host))
        self.pkg_config_path = op.join(self.prefix, "lib", "pkgconfig")
        self.externals = externals

        self.cpu = args.host_arch
        self.cpu_family = args.host_arch

        if args.host == "Android":
            self.android_ndk_home = os.getenv("ANDROID_NDK_HOME")
            if not self.android_ndk_home:
                raise Exception("ANDROID_NDK_HOME is not set")

            self.android_abi = _ANDROID_ABI_MAP[args.host_arch]
            self.android_compiler = _ANDROID_COMPILER_MAP[args.host_arch]
            self.android_ndk_build = _ANDROID_BUILD_MAP[_SYSTEM]

            self.android_ndk_bin = op.join(
                self.android_ndk_home, "toolchains", "llvm", "prebuilt", self.android_ndk_build, "bin"
            )
            self.android_cmake_toolchain = op.join(self.android_ndk_home, "build", "cmake", "android.toolchain.cmake")

        if self.host == "Windows":
            if "gpu_capture" in args.debug_opts:
                _add_prerequisite(self, _nopegl_setup, _renderdoc_install)

    def get_env(self):
        sep = ":" if _SYSTEM == "MinGW" else os.pathsep
        env = {}
        env["PATH"] = sep.join((self.venv_bin_path, "$(PATH)"))
        env["PKG_CONFIG_PATH"] = self.pkg_config_path
        if self.host == "Android":
            env["PKG_CONFIG_LIBDIR"] = self.pkg_config_path
            env["CMAKE_PREFIX_PATH"] = op.join(self.prefix, "lib", "cmake")
        if self.host == "iOS":
            env["PKG_CONFIG_LIBDIR"] = self.pkg_config_path
            env["CMAKE_PREFIX_PATH"] = op.join(self.prefix, "lib", "cmake")
        elif self.host == "Windows":
            env["PKG_CONFIG_ALLOW_SYSTEM_LIBS"] = "1"
            env["PKG_CONFIG_ALLOW_SYSTEM_CFLAGS"] = "1"
            env["CMAKE_PREFIX_PATH"] = op.join(self.prefix, "cmake")
        return env


def _build_env_cross_file(cfg):
    if cfg.args.host == "Android":
        _gen_android_cross_file(cfg)
    elif cfg.args.host == "iOS":
        _gen_ios_cross_file(cfg)


def _build_env_scripts(cfg):
    if _SYSTEM == "Windows":
        activate_path = os.path.join(cfg.venv_bin_path, "Activate.ps1")
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
            $Env:NGL_DLL_DIRS="{cfg.venv_bin_path}"
            """
        )
        with open(op.join(cfg.venv_bin_path, "ngli-activate.ps1"), "w") as fp:
            fp.write(ngl_activate_ps1)

        return

    activate_path = os.path.join(cfg.venv_bin_path, "activate")
    ngl_activate = textwrap.dedent(
        f"""\
        ngli_deactivate() {{
            deactivate
            unset NGL_DLL_DIRS
            unset -f ngli_deactivate
        }}

        . {activate_path}
        export NGL_DLL_DIRS="{cfg.venv_bin_path}"
        """
    )
    with open(op.join(cfg.venv_bin_path, "ngli-activate"), "w") as fp:
        fp.write(ngl_activate)


def _run():
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
    parser.add_argument("--build-backend", choices=("ninja", "vs"), default="ninja", help="Build backend to use")
    parser.add_argument(
        "--download-only",
        action=argparse.BooleanOptionalAction,
        help="Only download the external dependencies",
    )
    parser.add_argument("--host", choices=("Android", "iOS"), default=None, help="Cross compilation host machine")
    parser.add_argument(
        "--host-arch",
        choices=_CPU_FAMILIES,
        default="aarch64",
        help="Cross compilation host machine architecture",
    )

    args = parser.parse_args()

    logging.basicConfig(level="INFO")

    externals = _fetch_externals(args)
    if args.download_only:
        return

    if args.host == "Android" and _SYSTEM not in {"Linux", "Darwin"}:
        raise NotImplementedError(f"Android cross-build is not supported on {_SYSTEM}")
    elif args.host == "iOS" and _SYSTEM not in {"Darwin"}:
        raise NotImplementedError(f"iOS cross-build is not supported on {_SYSTEM}")

    _build_venv(args)

    cfg = _Config(args, externals)

    blocks = [
        _all,
        _clean,
    ]

    if _is_local(cfg.host):
        blocks += [
            _tests,
            _nopegl_updatedoc,
            _nopegl_updatespecs,
            _nopegl_updateglwrappers,
            _ngl_tools_install_nosetup,
            _htmldoc,
        ]

    if args.coverage:
        blocks += [_coverage_html, _coverage_xml]

    dst_makefile = op.join(_ROOTDIR, "Makefile")
    if args.host:
        dst_makefile += f".{args.host}"
        if args.host_arch:
            dst_makefile += f".{args.host_arch}"
    logging.info("writing %s", dst_makefile)
    makefile = _get_makefile(cfg, blocks)
    with open(dst_makefile, "w") as f:
        f.write(makefile)

    _build_env_cross_file(cfg)
    _build_env_scripts(cfg)


if __name__ == "__main__":
    _run()
