#!/bin/sh -xeu

self=$(readlink -f "$0")
resources_dir=$(dirname "$self")

if [ ! -f "configure.py" ]; then
    echo "$0 must be executed from the project root directory"
    exit 1
fi

target=$1
target_exec=
case "$target" in
    "nope-diff")
        target_exec="ngl-diff"
        ;;
    "nope-viewer")
        target_exec="ngl-viewer"
        ;;
    *)
        echo "usage: $0 {nope-viewer,nope-diff}"
        exit 1
        ;;
esac

# Check if repository is in a clean state
git diff-index --quiet HEAD
[ -f Makefile ] && exit 1
[ -d venv ] && exit 1

./configure.py --download-only

python -m venv tmp-venv
. tmp-venv/bin/activate

pip install ninja meson

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

$pip_install -r "$here/python/pynopegl/requirements.txt"
$pip_install "$here/python/pynopegl/"

$pip_install -r "$here/python/pynopegl-utils/requirements.txt"
$pip_install "$here/python/pynopegl-utils/"

$python -m pip uninstall -y cython

curl -sL https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage -o linuxdeploy
chmod +x linuxdeploy

sed "s/@TOOL@/$target_exec/g" "$resources_dir/AppRun.in" > "$resources_dir/AppRun"

LD_LIBRARY_PATH="$appdir/usr/lib/" ./linuxdeploy \
    --appdir "$appdir" \
    --output appimage \
    --custom-apprun "$resources_dir/AppRun" \
    --desktop-file "$resources_dir/$target.desktop" \
    --icon-file "$resources_dir/$target.png" \
    --executable "$python" \
    --executable "$appdir/usr/bin/ffmpeg" \
    --executable "$appdir/usr/bin/ffprobe" \
    --exclude-library libva.so.2 \
    --exclude-library libva-x11.so.2 \
    --exclude-library libva-wayland.so.2 \
    --exclude-library libva-drm.so.2 \
