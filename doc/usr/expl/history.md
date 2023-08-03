# History

This project was initially developed in 2016 at a French startup company named
Stupeflix. It took inspiration from the scene graph paradigm of [OSG] game
engine for which many clumsy in-house patches were maintained, essentially for
video playback support and lazy resources management.

The video playback logic was the first foundation to be extracted from these
patches, and landed in a dedicated module named [sxplayer] (standing for
"Stupeflix Player"), a free (LGPL) video playback library on top of FFmpeg.

While it helped with maintenance, most of the OSG features remained unneeded,
the custom Python bindings were unmaintained, painful to work with and tied to
a specific version of OSG, preventing us from using a more recent version. The
stack of patches added on top of the project was also tied to the same specific
version of OSG and was particularly challenging to rebase due to its complexity.
It was decided to prototype a new engine, more oriented toward video compositing
rather than video games. Also, mobile platforms (iOS and Android) became first
class citizen because mobiles applications were the main business target.

The "node.gl" name was selected, merely as a joke and word play with "node.js"
and OpenGL, along with the fact that the scene graph is composed of "nodes".

The project was open-sourced under Apache license when the [GoPro] company
acquired Stupeflix (late 2016). The main authors of the project, Clément Bœsch
and Matthieu Bouron, carried on with its development for years, making it a core
element of the GoPro Quik mobile editing application.

Along the years, the project matured into a significant engine for video
compositing, animations, video playback, color management, and was notably
entirely redesigned to support more modern graphics backends.

Around the end of 2022 and beginning of 2023, Matthieu and Clément left the
GoPro company and decided to maintain the project under a new name: the **Nope
Project**. `sxplayer` and `node.gl` were respectively renamed to `nope.media`
and `nope.gl`. The direction of the project then took a new turn, focusing on
providing a free and open-source framework for motion design, 2D composition
and visual effects.

[OSG]: https://www.openscenegraph.com "Open Scene Graph"
[sxplayer]: https://github.com/gopro/sxplayer "sxplayer"
[GoPro]: https://gopro.com "GoPro"
