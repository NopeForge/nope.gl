# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

This project adheres to a flavour of [Calendar Versioning](https://calver.org/)
for the globale releases (`YYYY.MINOR`), and to [Semantic
Versioning](https://semver.org/spec/v2.0.0.html) for `libnopegl`.

## [Unreleased]
### Added
- Much more detailed typing annotations in `pynopegl`
- `RenderDisplace` node for texture displacement
- Visual scopes support through the `ColorStats`, `RenderHistogram` and
  `RenderWaveform` nodes (see `scopes` example in the demos)
- Multiple modulo functions for the `Eval*` nodes: `mod_e` (euclidean), `mod_f`
  (floored) and `mod_t` (truncated)

### Changed
- CSV export in the HUD now always prints floats in C locale instead of quoted
- `pynopegl.Context.configure()` now takes a `Config` as argument
- `pynopegl` log levels are now controled using the `pynopegl.Log` enum
- `max_texture_dimensions_*` capabilities are renamed to `max_texture_dimension_*`
- Backend probing in `pynopegl` now returns a more Pythonic output

### Removed
- `ResourceProps.variadic` bool flag as it was never a functional interface

## [2023.1] [libnopegl 0.8.0] - 2023-04-07
### Changed
- Project renamed to `nope.gl`, as part of the Nope Project

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
