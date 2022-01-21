# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

This project adheres to a flavour of [Calendar Versioning](https://calver.org/)
for the globale releases (`YYYY.MINOR`), and to [Semantic
Versioning](https://semver.org/spec/v2.0.0.html) for `libnodegl`.

## [Unreleased]
### Added
- TextureView node, useful for targeting a specific layer of a texture in RTT
  scenarios
- Initial Vulkan support
- VAAPI/Vulkan video acceleration support
- Metal support through MoltenVK, including VideoToolbox video acceleration

### Fixed
- Fix OpenGL buffers and textures bindings
- A memory crash happening when resetting the context scene to NULL with the
  HUD active

## [2022.1] [libnodegl 0.1.0] - 2022-01-20
### Added
- This Changelog
