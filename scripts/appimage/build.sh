#!/bin/sh -xeu

self=$(readlink -f "$0")
resources_dir=$(dirname "$self")

if [ ! -f "configure.py" ]; then
    echo "$0 must be executed from the project root directory"
    exit 1
fi

# Check if repository is in a clean state
git diff-index --quiet HEAD
[ -f Makefile ] && exit 1
[ -d venv ] && exit 1

./configure.py --download-only

pip install --user ninja meson
export PATH="$HOME/.local/bin:$PATH"

here="$(pwd)"
appimagedir="$here/appimage"
appdir="$appimagedir/appdir"
python_ver=3.10
python="$appdir/usr/bin/python$python_ver"
pip_install="$python -m pip -v install --no-warn-script-location"

rm -rf "$appimagedir"
mkdir "$appimagedir"
cd "$appimagedir"

# Create appdir and install Python libraries
export DEBIAN_FRONTEND=noninteractive

mkdir "$appdir"
cd "$appdir"
apt download                     \
    libpython$python_ver-stdlib  \
    libpython$python_ver-minimal \
    python$python_ver            \
    python$python_ver-minimal    \
    python3-distutils            \
    python3-pip                  \
    ffmpeg                       \

find . -name "*.deb" -exec dpkg-deb -x {} . \;
find . -name "*.deb" -delete
cd "$appimagedir"

meson_setup="meson setup \
    --prefix $appdir/usr \
    --pkg-config-path $appdir/usr/lib/pkgconfig \
    --buildtype release \
    --libdir=lib \
    --backend=ninja \
    -Db_lto=true \
    -Drpath=true"


builddir="$appimagedir/builddir"

$meson_setup "$here/external/nopemd" "$builddir/nmd"
meson install -C "$builddir/nmd"

$meson_setup "$here/libnopegl" "$builddir/ngl"
meson install -C "$builddir/ngl"

export PKG_CONFIG_PATH="$appdir/usr/lib/pkgconfig"

$pip_install setuptools cython

$pip_install -r "$here/pynopegl/requirements.txt"
$pip_install "$here/pynopegl/"

$pip_install -r "$here/pynopegl-utils/requirements.txt"
$pip_install "$here/pynopegl-utils/"

$python -m pip uninstall -y cython

curl -sL https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage -o linuxdeploy
chmod +x linuxdeploy

LD_LIBRARY_PATH="$appdir/usr/lib/" ./linuxdeploy \
    --appdir "$appdir" \
    --output appimage \
    --custom-apprun "$resources_dir/AppRun" \
    --desktop-file "$resources_dir/nope-viewer.desktop" \
    --icon-file "$resources_dir/nope-viewer.png" \
    --executable "$python" \
    --executable "$appdir/usr/bin/ffmpeg" \
    --executable "$appdir/usr/bin/ffprobe" \
    --exclude-library libva.so.2 \
    --exclude-library libva-x11.so.2 \
    --exclude-library libva-wayland.so.2 \
    --exclude-library libva-drm.so.2 \
