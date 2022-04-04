# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

This project adheres to a flavour of [Calendar Versioning](https://calver.org/)
for the globale releases (`YYYY.MINOR`), and to [Semantic
Versioning](https://semver.org/spec/v2.0.0.html) for `libnodegl`.

## [Unreleased]
### Added
- `eye`, `center` and `up` Camera parameters can now be node
- Honor [NO_COLOR](https://no-color.org/) environment variable

### Fixed
- A crash happening in the VAAPI context module when both the OpenGL and Vulkan
  backends were enabled.
- A deadlock in `ngl-control` when specifying invalid encoding arguments

## [2022.2] [libnodegl 0.2.0] - 2022-03-28
### Added
- TextureView node, useful for targeting a specific layer of a texture in RTT
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
