# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

This project adheres to a flavour of [Calendar Versioning](https://calver.org/)
for the globale releases (`YYYY.MINOR`), and to [Semantic
Versioning](https://semver.org/spec/v2.0.0.html) for `libnopegl`.

## [Unreleased]
### Added
- `ngl.SceneCfg` now includes the backend capabilities
- `%s_coord_matrix` uniform is now exposed for 2D array and 3D textures
- `ngl_get_backend()` function to retrieve the backend information associated
  with a configured `nope.gl` context
- `ngl-diff` can now set and change the input files from the GUI. While still
  supported, passing them through the command line is not mandatory anymore
- `ngl_scene_ref` and `ngl_scene_unrefp` functions to respectively increment and
  decrement the reference counter of a scene

### Fixed
- Moving the split position in `ngl-diff`
- Crash in the hwconv module when direct rendering is not possible/enabled
- Export to output files containing spaces in their path on Windows

### Changed
- `ngl.get_backends()` and `ngl.probe_backends()` were mistakenly inverted in
  v2023.2, this is changed back in this release
- `SceneCfg` and `SceneInfo` now use the `Backend` enum instead of a string
- `ngl_configure()` does not update the `backend` field from the user
  configuration anymore and the `config` argument is now const. Users must now
  use `ngl_get_backend()` to get the underlying backend information
- `ngl_scene_init_from_node()` has been replaced with
  `ngl_scene_init()` with the associated `ngl_scene_params` structure
- the `ngl_scene` structure is now private; its parameters can now be obtained
  using `ngl_scene_get_params()`

### Removed
- `%s_dimensions` uniform for 2D array and 3D images/textures, users must use
  textureSize()/imageSize() instead (`%s_dimensions` is still available for 2D
  textures)

### Removed
- `ngl_scene_freep` function; use `ngl_scene_unrefp` instead

## [2023.4] [libnopegl 0.10.0] - 2023-09-28
### Added
- `Eval*` nodes now accept multi-dimensional input resources
- `luma(r,g,b)` and `srgbmix(a,b,x)` functions in eval
- `GridLayout` node to make a grid out of a number of scenes
- `FilterColorMap` node to remap colors using a gradient of color points
- Animated GIF export in the viewer
- `RenderNoise` node to generate fractal noise on the GPU
- `Texture2D` node now accepts a scene as `data_src` and acts as an implicit
  render target (which simplifies the graph when all the features of the
  `RenderToTexture` node are not needed)
- `RenderToTexture` and `Texture2D` nodes can now be automatically resized to
  match screen size if their initial dimensions are left to (0, 0)
- Ability to drag'n drop Python scripts in the Viewer

### Fixed
- Viewer path management on Windows
- Name filters in the export dialog of the viewer
- Viewer export on Windows

### Changed
- The installed `nodes.specs` specifications have been extended to include more
  information (types, choices, etc)
- The documentation has been reworked, notably to include code examples with
  associated rendering and graph
- `Transform` node now accepts a node as input parameter and can be animated
  this way
- `scene`, `SceneInfo` and `SceneCfg` are now part of `pynopegl` instead of
  `pynopegl-utils`, where they are deprecated

### Removed
- `RenderToTexture.features` as the RTT node is now able to detect
  automatically if the underlying graph requires a depth/stencil buffer

## [2023.3] [libnopegl 0.9.0] - 2023-09-01
### Fixed
- Linux and macOS release jobs

## [2023.2] [libnopegl 0.9.0] - 2023-09-01
### Added
- Much more detailed typing annotations in `pynopegl`
- `RenderDisplace` node for texture displacement
- Visual scopes support through the `ColorStats`, `RenderHistogram` and
  `RenderWaveform` nodes (see `scopes` example in the demos)
- Multiple modulo functions for the `Eval*` nodes: `mod_e` (euclidean), `mod_f`
  (floored) and `mod_t` (truncated)
- Support for rendering to 3D textures
- Support for 3D textures image load/store
- Support for cube maps image load/store
- Support for array of 2D textures
- External font rendering in the `Text` node
- Fixed scaling in the `Text` node
- Text effects (color, opacity, transform, glow, outline, blur), applicable per
  character/word/lines
- `PathKeyClose` node to close the current sub-path
- `RenderPath` node to render paths visually
- Optional `compat_specs` in the `@scene` decorator to add a version requirement
  constraint
- `ngl-viewer` tool to select, customize and export scenes with a simple and
  intuitive interface

### Changed
- CSV export in the HUD now always prints floats in C locale instead of quoted
- `pynopegl.Context.configure()` now takes a `Config` as argument
- `pynopegl` log levels are now controled using the `pynopegl.Log` enum
- `max_texture_dimensions_*` capabilities are renamed to `max_texture_dimension_*`
- Backend probing in `pynopegl` now returns a more Pythonic output
- `GraphicConfig.scissor` is now an `ivec4` parameter instead of `vec4`
- `TimeRangeMode*` nodes are removed and `TimeRangeFilter` is simplified to
  handle their role; if multiple ranges are needed simulteanously, a `Group` can
  be used. `TimeRangeFilter.max_idle_time` is removed since its use was limited
  to inter range times (now only a single time segment is handled).
- The API now mandates that node scenes have to be wrapped into a dedicated
  scene object that holds extra metadata (duration, aspect ratio and framerate)
- All counters and size-based arguments in the public API are now using `size_t`
  instead of `int`
- The `Text.valign` and `Text.halign` parameters now also align the text per line
- `Text` rendering has been completely reworked to support more advanced effects
  and smooth rendering at high resolution
- The `@scene` decorator now relies on an explicity `controls` dictionary
  instead of `**kwargs`
- The `Texture*` nodes now use `linear` filtering by default instead of
  `nearest`

### Removed
- `ResourceProps.variadic` bool flag as it was never a functional interface
- `NGL_CAP_NPOT_TEXTURE` as we mandate full support for textures with
   non-power-of-two dimensions
- Support for OpenGL < 3.3 and OpenGL ES < 3.0 and the associated capabilities:
  `NGL_CAP_BLOCK`, `NGL_CAP_INSTANCED_DRAW`, `NGL_CAP_SHADER_TEXTURE_LOD`,
  `NGL_CAP_TEXTURE_2D_ARRAY`, `NGL_CAP_TEXTURE_3D`, `NGL_CAP_TEXTURE_CUBE`,
  `NGL_CAP_UINT_UNIFORMS`
- GLSL `ngl_tex{2d,3d,cube,2dlod,3dlod,cubelod}()` declarations (users must now
  use `texture()` and `textureLod()` directly). `ngl_texvideo()` is still there
  and is still the prefered picking method

## [2023.1] [libnopegl 0.8.0] - 2023-04-07
### Changed
- Project renamed to `nope.gl`

----

## [2023.0] [libnodegl 0.7.0] - 2023-03-09
### Fixed
- Color channel difference in `ngl-diff` is now done in linear space

### Added
- Windows DLL information (Copyrights, Version, Name)

### Changed
- The installed `nodes.specs` is now in `JSON` instead of `YAML`
- The default branch is now named `main`

## [2022.8] [libnodegl 0.6.1] - 2022-09-22
### Fixed
- Crash with specific draw time sequences in some time filtered diamond-tree
  graphs involving `TimeRangeFilter` nodes keeping some parts of the graph active
- Build `setuptools>=60` on MinGW
- Hooks file synchronization for file names with special chars
- A race condition in `ngl-control` reload mechanism

### Added
- `ngl-diff` can now open images

### Changed
- Improved `ngl-diff` color map

## [2022.7] [libnodegl 0.6.0] - 2022-06-07
### Added
- HDR tone mapping support for PQ content

### Fixed
- Visual glitches when seeking with the VAAPI/Vulkan acceleration
- Crash with specific draw time sequences in some time filtered diamond-tree
  graphs
- Memory leak with `Buffer*(count=N)` (only when no data is specified)

### Changed
- Handle OpenGLES as a separate backend build-wise

## [2022.6] [libnodegl 0.5.0] - 2022-05-19
### Added
- Mediacodec/Vulkan video acceleration support
- EGL device platform support (allows to support NVIDIA GPUs in headless
  environments)
- Compatibility with static analyzers for symbols discovery in `pynodegl`,
  including typing annotations
- Compatibility with Vulkan devices not supporting host cached memory

### Fixed
- Missing exposed constants in `pynodegl` (some backend capabilities and
  the Wayland platform ID)
- Undefined behaviour when using geometries with `Render*` nodes (`Render` node
  is not affected)
- Undefined behaviour in sRGB/linear conversions with negative colors

### Changed
- `Buffer.block_field` is now a string corresponding to the field name instead
  of an integer index

## [2022.5] [libnodegl 0.4.0] - 2022-05-06
### Fixed
- Missing package data files for `ngl-diff`

## [2022.4] [libnodegl 0.4.0] - 2022-05-06
### Added
- Optional `backend_config` field to the `ngl_config` structure (ABI break)
- External OpenGL context support
- `ngl-diff` tool to visually compare videos
- `UniformColor.space` colorspace configuration (sRGB, HSL, HSV)
- `AnimatedColor` and `AnimKeyFrameColor` nodes
- `set_surface_pts` support to the Vulkan backend (Android)

### Fixed
- Various memory leaks
- Hooks session information sometimes not being updated in `ngl-control`

### Removed
- `handle` field from the `ngl_config` structure
- `UniformColorA` (use `UniformColor` instead, with a separate `UniformFloat`
  for the opacity)

## [2022.3] [libnodegl 0.3.0] - 2022-04-11
### Added
- `eye`, `center` and `up` Camera parameters can now be node
- Honor [NO_COLOR](https://no-color.org/) environment variable

### Fixed
- A crash happening in the VAAPI context module when both the OpenGL and Vulkan
  backends were enabled.
- A deadlock in `ngl-control` when specifying invalid encoding arguments

## [2022.2] [libnodegl 0.2.0] - 2022-03-28
### Added
- `TextureView` node, useful for targeting a specific layer of a texture in RTT
  scenarios
- Initial Vulkan support
- VAAPI/Vulkan video acceleration support
- Metal support through MoltenVK, including VideoToolbox video acceleration
- HDR tone mapping support for HLG content

### Fixed
- Fix OpenGL buffers and textures bindings
- A memory crash happening when resetting the context scene to NULL with the
  HUD active

## [2022.1] [libnodegl 0.1.0] - 2022-01-20
### Added
- This Changelog
