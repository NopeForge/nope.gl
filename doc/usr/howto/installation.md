# Installation

## Download

The sources can be downloaded on the [NopeForge/nope.gl GitHub repository][nopegl],
either by downloading a zip snapshot or cloning them with `git`:

```sh
git clone https://github.com/NopeForge/nope.gl
```

[nopegl]: https://github.com/NopeForge/nope.gl


## Method 1: quick user build & installation

This page includes the system requirements as well as the steps needed for
building and running the complete `nope.gl` stack.

### Linux and MacOS

- Install the needed dependencies:
  - **GCC** or **Clang**
  - **GNU/Make**
  - **FFmpeg** (and its libraries for compilation)
  - **Python ≥ 3.8** (you will need the package with the build headers as well,
    typically named with a `-dev` suffix on Debian based systems)
  - **Graphviz**
  - **SDL2**
- Build with `./configure.py && make`
- Enter the virtual environment with `. venv/bin/ngli-activate`

### Windows (MinGW64 toolchain)

- Install [MSYS2] (which also brings **MinGW64**)
- Install dependencies via pacman using **MinGW64** shell (*Not* MSYS2,
"MINGW64" should be visible in the prompt):
    ```shell
    pacman -Syuu  # and restart the shell
    pacman -S --needed git make mingw-w64-ucrt-x86_64-{toolchain,ffmpeg,python,python-watchdog,python3-pillow,pyside6,meson,graphviz,vulkan-devel}
    pacman -S --needed mingw-w64-ucrt-x86_64-ca-certificates
    ```
- From MinGW64, build with `./configure.py && make`
- Enter the virtual environment with `. venv/bin/ngli-activate`

[MSYS2]: https://www.msys2.org/

### Windows (MSVC toolchain)

- Install [Python][python-win] ≥ 3.8 using the Windows Installer and make
  sure to select the option to add Python to the system `PATH` during its
  installation
- Install [Graphviz][graphviz] using the Windows Installer and make sure to
  select the option to add Graphviz to the system `PATH` during its installation
- Install [Microsoft Visual Studio][vs] and make sure the *Desktop development
  with C++* is included (it should enable MSVC build tools and Windows 10 SDK).
- Install [VulkanSDK][vksdk] (optional)
- To be allowed to run the build scripts, you will need to run this once in an
  administrator Powershell: `Set-ExecutionPolicy RemoteSigned`
- In a Powershell (as user), in the `nope.gl` sources, run
  `.\scripts\msvc-env.ps1` to import all the necessary variables for the MSVC
  environment. Alternatively, you can open *VS Native Tools Prompt* and spawn a
  `powershell` from here.
- Finally, run `python.exe .\configure.py` followed by `nmake`

[python-win]: https://www.python.org/downloads/windows/
[graphviz]: https://graphviz.org/download/
[vs]: https://visualstudio.microsoft.com/downloads/
[vksdk]: https://vulkan.lunarg.com/sdk/home#windows


## Method 2: manual components installation

This section is targeted at packagers willing to build packages for every
individual components. The requirements are the same as the first method.

### `libnopegl` (the core library)

`libnopegl` uses [Meson][meson] for its build system. Its compilation and
installation usually looks like the following:

```sh
meson setup builddir
meson compile -C builddir
meson install -C builddir
```

`meson configure` can be used to list the available options. See the [Meson
documentation][meson-doc] for more information.

[meson]: https://mesonbuild.com/
[meson-doc]: https://mesonbuild.com/Quick-guide.html#compiling-a-meson-project

### `ngl-tools`

The `nope.gl` tools located in the `ngl-tools/` directory are to be built and
installed exactly the same way as `libnopegl`.

### `pynopegl` (the Python binding)

```shell
pip install -r ./pynopegl/requirements.txt
pip install ./pynopegl
```

### `pynopegl-utils` (the Python utilities and examples)

```shell
pip install -r ./pynopegl-utils/requirements.txt
pip install ./pynopegl-utils
```
