How the Python binding is created
=================================

## Simplified overview

```{mermaid}
graph

    libnopegl_install(libnopegl install)
    pynopegl_setup(pynopegl setup.py)

    cython(Cython)
    cc(C compiler)

    specs(nodes.specs)
    c(_pynopegl.c)
    pyx(nodes_def.pyx)

    mod(_pynopegl.so)

    pymod(pynopegl)

    libnopegl_install -- write --> specs
    pynopegl_setup -- read --> specs
    pynopegl_setup -- write --> pyx
    cython -- read --> pyx
    cython -- write --> c
    cc -- read --> c
    cc -- write --> mod
    pymod -- import --> mod

    classDef installers fill:#8ee5ee,color:#222
    classDef ctools     fill:#ee6aa7,color:#222
    classDef files      fill:#7cfc00,color:#222

    class libnopegl_install,pynopegl_setup installers
    class cython,cc ctools
    class specs,c,pyx files
    style mod fill:#ffd700,color:#222
    style pymod fill:#ff7f50,color:#222
```

## Detailed steps

### libnopegl → nodes.specs

`libnopegl` writes the [nodes.specs][specs] specifications in JSON using a
dedicated tool ([gen_specs.c][gen-specs-c]) crawling the internal node C
definitions.

This generated `nodes.specs` file (see `updatespecs` build rule in libnopegl's
generated `Makefile`) is installed on the system or targeted environment by the
`install` rule.

[specs]: source:libnopegl/nodes.specs
[gen-specs-c]: source:libnopegl/gen_specs.c

### nodes.specs ← pynopegl

In its [setup.py][pynopegl-setup], `pynopegl` uses `pkg-config` to query the
`libnopegl` installation data directory, in order to obtain the path to the
installed `nodes.specs` file. The file is then loaded as JSON file.

[pynopegl-setup]: source:pynopegl/setup.py

### pynopegl → nodes_def.pyx

Using the loaded `nodes.specs` as JSON, `setup.py` writes the Cython code
definitions into a `nodes_def.pyx` file.

### nodes_def.pyx ← Cython

The `setuptools` module used to package `pynopegl` calls Cython to read the
generated `.pyx` file (along with [_pynopegl.pyx][pynopegl-pyx]).

### Cython → _pynopegl.c

From the `.pyx` files, Cython will generate a C source code using the C
Python API. This source file declares the Python classes calling the libnopegl
C functions, based on the rules contained in the `.pyx` files.

### _pynopegl.c ← C compiler

In the Cython toolchain triggered by the `setuptools`, a C compiler will compile
the generated C source.

### C compiler → _pynopegl.so

Compiled source ends up being linked against Python library to create a
`_pynopegl.so` loadable Python module.

[pynopegl-pyx]: source:pynopegl/_pynopegl.pyx

### pynopegl ← _pynopegl.so

The final binding exposed to the user is a traditional pure Python module,
which imports the native `_pynopegl` module. All the nodes and their methods
are dynamically generated into that module at runtime.
