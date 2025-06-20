# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

This project adheres to a flavor of [Calendar Versioning](https://calver.org/)
for the global releases (`YYYY.MINOR`), and to [Semantic
Versioning](https://semver.org/spec/v2.0.0.html) for `libnopegl`.

## [Unreleased]
### Added
- `ngl_scene_get_filepaths()` and `ngl_scene_update_filepath()` to introspect
  and manipulate the filepath based parameters in the scene graph
- Live change support for `Media.filename`
- `ngl_get_viewport()` to get the viewport currently in use by the rendering
  context
- `FontFace.index` parameter to select a different face in the font file
- `DrawTexture.texture` now accepts transformation nodes before the texture node
  to serve as a reframing mechanism (the transforms are applied to the texture
  coordinates in a centered `[-1,1]` space with `(-1,-1)` in the bottom left)
- `HexagonalBlur` node to apply a post processing hexagonal bokeh blur effect to a
  texture
- `TextEffect.anchor` and `TextEffect.anchor_ref` to control the character
  relative anchor for scale and rotate transforms
- `DrawMask` node to facilitate alpha masking with textures
- `ngl_config.debug` parameter to enable graphics context debugging
- `ngl-export` tool to export videos for all the scenes from a given script
- Path and text rendering can now control the position of the outline (inner,
  centered, outer, or anything in between) through the `outline_pos` parameter

### Fixed
- Crash when using resizable RTTs with time ranges
- Path and text blur rendering breaking anti-aliasing with small values

### Changed
- `Text.font_files` text-based parameter is replaced with `Text.font_faces` node
  list which accepts `FontFace` nodes instead
- The `ngl_config` structure and the `ngl_resize()` function do not have a
  `viewport` anymore; it is now automatically calculated internally according to
  the selected resolution and the scene aspect ratio
- `Text.box_{corner,width,height}` are replaced with a 4 component long
  `Text.box` parameter; if 3D positioning is required, traditional transform
  nodes can be used
- `Render*` nodes are renamed to `Draw*`
- `TextEffect.transform` now default to an anchor in the center of the
  characters instead of a distant bottom-left position
- `TextEffect.transform` are now combined on overlapping text effects instead of
  replacing the previous one
- Public anonymous enums (`ngl_log_level`, `ngl_backend_type`,
  `ngl_platform_type`, `ngl_capture_buffer_type`) are now named
- `GraphicConfig.depth_write_mask` is renamed to `GraphicConfig.depth_write`
- `Circle.count`, `Buffer*.count`, `StreamedBuffer*.count` are now unsigned
- `Program.nb_frag_output` is now unsigned
- The `libnopegl` headers are now located in the nopegl sub directory. Users
  must now use `#include <nopegl/nopegl.h>` instead of `#include <nopegl.h>`
- All blur nodes now have a common `blurriness` parameter
- Blur effect and outline width in text and path rendering are now twice smaller
- The glow effect of the text and path is reworked; it notably lighten up the
  whole shape and emits less light
- Path rendering doesn't fill the shape with color anymore when the path is
  opened
- Text outline is now by default on the outer edge, and thus doesn't affect the
  shape of the characters anymore
- `Texture2D.{width,height}`, `Texture2DArray.{width,height,depth}`,
  `Texture3d.{width,height,depth}`, `TextureCube.size`, `TextureView.layer` are
  now unsigned
- `ngl_config.{width,height,samples}` and `ngl_resize()` arguments are now unsigned

### Removed
- `Text.aspect_ratio`, it now matches the viewport aspect ratio
- `GraphicConfig.scissor_test` parameter
- Support for Android < 9.0
- The `small` build option (enabled by default now)

## [2024.0 / libnopegl 0.11.0][2024.0] - 2024-02-02
### Added
- `ngl.SceneCfg` now includes the backend capabilities
- `%s_coord_matrix` uniform is now exposed for 2D array and 3D textures
- `ngl_get_backend()` function to retrieve the backend information associated
  with a configured `nope.gl` context
- `ngl-diff` can now set and change the input files from the GUI. While still
  supported, passing them through the command line is not mandatory anymore
- `ngl_scene_ref` and `ngl_scene_unrefp` functions to respectively increment and
  decrement the reference counter of a scene
- `GaussianBlur` node to apply a post processing Gaussian blur effect to a
  scene with a resolution dependent blurriness parameter. The blurriness
  parameter can provide a Gaussian blur effect with a radius up to 126 pixels
- `FastGaussianBlur` node to apply a post processing Gaussian blur effect to a
  scene that is suitable for real time rendering on mobile devices as well as
  providing a resolution independent blurriness parameter
- `forward_transforms` parameter to the `Texture` and `RenderToTexture` nodes
  to enable forwarding of the camera/model transformations
- `Selector` filter to select colors according to their lightness, chroma or hue
- `AnimKeyFrame*` nodes can now be reused at a different time using the
  `Animated*.time_offset` parameter
- `TextEffect.end_time` now automatically uses the scene duration when
  unspecified

### Fixed
- Moving the split position in `ngl-diff`
- Crash in the hwconv module when direct rendering is not possible/enabled
- Export to output files containing spaces in their path on Windows
- Crash when decoding videos on Android with the OpenGLES backend

### Changed
- `ngl.get_backends()` and `ngl.probe_backends()` were mistakenly inverted in
  v2023.2, this is changed back in this release
- `SceneCfg` and `SceneInfo` now use the `Backend` enum instead of a string
- `ngl_configure()` does not update the `backend` field from the user
  configuration anymore and the `config` argument is now const. Users must now
  use `ngl_get_backend()` to get the underlying backend information
- `ngl_scene_init_from_node()` has been replaced with
  `ngl_scene_init()` with the associated `ngl_scene_params` structure
- The `ngl_scene` structure is now private; its parameters can now be obtained
  using `ngl_scene_get_params()`
- The `Texture` and `RenderToTexture` nodes no longer forward the camera/model
  transformations by default

### Removed
- `%s_dimensions` uniform for 2D array and 3D images/textures, users must use
  `textureSize()`/`imageSize()` instead (`%s_dimensions` is still available for
  2D textures)
- `SceneInfo.files` and `SceneCfg.files`

### Removed
- `ngl_scene_freep` function; use `ngl_scene_unrefp` instead

## [2023.4 / libnopegl 0.10.0][2023.4] - 2023-09-28
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
  information (types, choices, etc.)
- The documentation has been reworked, notably to include code examples with
  associated rendering and graph
- `Transform` node now accepts a node as input parameter and can be animated
  this way
- `scene`, `SceneInfo` and `SceneCfg` are now part of `pynopegl` instead of
  `pynopegl-utils`, where they are deprecated

### Removed
- `RenderToTexture.features` as the RTT node is now able to detect
  automatically if the underlying graph requires a depth/stencil buffer

## [2023.3 / libnopegl 0.9.0][2023.3] - 2023-09-01
### Fixed
- Linux and macOS release jobs

## [2023.2 / libnopegl 0.9.0][2023.2] - 2023-09-01
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
- `pynopegl` log levels are now controlled using the `pynopegl.Log` enum
- `max_texture_dimensions_*` capabilities are renamed to `max_texture_dimension_*`
- Backend probing in `pynopegl` now returns a more Pythonic output
- `GraphicConfig.scissor` is now an `ivec4` parameter instead of `vec4`
- `TimeRangeMode*` nodes are removed and `TimeRangeFilter` is simplified to
  handle their role; if multiple ranges are needed simultaneously, a `Group` can
  be used. `TimeRangeFilter.max_idle_time` is removed since its use was limited
  to inter range times (now only a single time segment is handled).
- The API now mandates that node scenes have to be wrapped into a dedicated
  scene object that holds extra metadata (duration, aspect ratio and framerate)
- All counters and size-based arguments in the public API are now using `size_t`
  instead of `int`
- The `Text.valign` and `Text.halign` parameters now also align the text per line
- `Text` rendering has been completely reworked to support more advanced effects
  and smooth rendering at high resolution
- The `@scene` decorator now relies on an explicitly `controls` dictionary
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
  and is still the preferred picking method

## [2023.1 / libnopegl 0.8.0][2023.1] - 2023-04-07
### Changed
- Project renamed to `nope.gl`

----

## [2023.0 / libnodegl 0.7.0][2023.0] - 2023-03-09
### Fixed
- Color channel difference in `ngl-diff` is now done in linear space

### Added
- Windows DLL information (Copyrights, Version, Name)

### Changed
- The installed `nodes.specs` is now in `JSON` instead of `YAML`
- The default branch is now named `main`

## [2022.8 / libnodegl 0.6.1][2022.8] - 2022-09-22
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

## [2022.7 / libnodegl 0.6.0][2022.7] - 2022-06-07
### Added
- HDR tone mapping support for PQ content

### Fixed
- Visual glitches when seeking with the VAAPI/Vulkan acceleration
- Crash with specific draw time sequences in some time filtered diamond-tree
  graphs
- Memory leak with `Buffer*(count=N)` (only when no data is specified)

### Changed
- Handle OpenGLES as a separate backend build-wise

## [2022.6 / libnodegl 0.5.0][2022.6] - 2022-05-19
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
- Undefined behavior when using geometries with `Render*` nodes (`Render` node
  is not affected)
- Undefined behavior in sRGB/linear conversions with negative colors

### Changed
- `Buffer.block_field` is now a string corresponding to the field name instead
  of an integer index

## [2022.5 / libnodegl 0.4.0][2022.5] - 2022-05-06
### Fixed
- Missing package data files for `ngl-diff`

## [2022.4 / libnodegl 0.4.0][2022.4] - 2022-05-06
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

## [2022.3 / libnodegl 0.3.0][2022.3] - 2022-04-11
### Added
- `eye`, `center` and `up` Camera parameters can now be node
- Honor [NO_COLOR](https://no-color.org/) environment variable

### Fixed
- A crash happening in the VAAPI context module when both the OpenGL and Vulkan
  backends were enabled.
- A deadlock in `ngl-control` when specifying invalid encoding arguments

## [2022.2 / libnodegl 0.2.0][2022.2] - 2022-03-28
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

## [2022.1 / libnodegl 0.1.0][2022.1] - 2022-01-20
### Added
- This Changelog

[2022.1]: https://github.com/NopeForge/nope.gl/releases/tag/v2022.1
[2022.2]: https://github.com/NopeForge/nope.gl/releases/tag/v2022.2
[2022.3]: https://github.com/NopeForge/nope.gl/releases/tag/v2022.3
[2022.4]: https://github.com/NopeForge/nope.gl/releases/tag/v2022.4
[2022.5]: https://github.com/NopeForge/nope.gl/releases/tag/v2022.5
[2022.6]: https://github.com/NopeForge/nope.gl/releases/tag/v2022.6
[2022.7]: https://github.com/NopeForge/nope.gl/releases/tag/v2022.7
[2022.8]: https://github.com/NopeForge/nope.gl/releases/tag/v2022.8
[2023.0]: https://github.com/NopeForge/nope.gl/releases/tag/v2023.0
[2023.1]: https://github.com/NopeForge/nope.gl/releases/tag/v2023.1
[2023.2]: https://github.com/NopeForge/nope.gl/releases/tag/v2023.2
[2023.3]: https://github.com/NopeForge/nope.gl/releases/tag/v2023.3
[2023.4]: https://github.com/NopeForge/nope.gl/releases/tag/v2023.4
[2024.0]: https://github.com/NopeForge/nope.gl/releases/tag/v2024.0
