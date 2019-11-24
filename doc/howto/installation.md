# Installation

## Quick user installation

For a fast ready-to-go user experience, you can use the default rule of the
root `Makefile`.  Calling `make` in the root directory of node.gl will create a
complete environment (based on Python virtualenv):

```shell
% make
[...]
    Install completed.

    You can now enter the venv with:
        source /home/user/node.gl/nodegl-env/bin/activate
```

## Installation of `libnodegl` (the core library)

### Build

`make` is enough to build `libnodegl.a`.

If you prefer a dynamic library, you can use the variable `SHARED`, such as
`make SHARED=yes`.

If you need symbol debugging, you can use `make DEBUG=yes`.

Make allow options to be combinable, so `make SHARED=yes DEBUG=yes` is valid.

Additionally, `PYTHON` and `PKG_CONFIG` which respectively allows to customize
`python` and `pkg-config` executable paths.

### Installation

`make install` will install the library in `PREFIX`, which you can override,
for example using `make install PREFIX=/tmp/local`.

You can check the installed version of `libnodegl` using `pkg-config
--modversion libnodegl`

## Installation of `ngl-tools`

The `node.gl` tools located in the `ngl-tools/` directory are to be built and
installed exactly the same way as `libnodegl`.

## Installation of `pynodegl` (the Python binding)

```shell
pip install -r ./pynodegl/requirements.txt
pip install ./pynodegl
```

## Installation of `pynodegl-utils` (the Python utilities and examples)

```shell
pip install -r ./pynodegl-utils/requirements.txt
pip install ./pynodegl-utils
```
