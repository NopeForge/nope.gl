node.gl
=======

`node.gl` is a [GoPro][gopro] OpenGL engine for building and rendering
graph-based scenes. It is designed to run on both desktop (Linux, OSX, Windows)
and mobile (Android, iOS).

![node.gl logo](/doc/nodegl.png)

**Warning:** note that `node.gl` is still highly experimental. This means the ABI
and API can change at any time.

![tests Linux](https://github.com/gopro/gopro-lib-node.gl/workflows/tests%20Linux/badge.svg)
![tests Mac](https://github.com/gopro/gopro-lib-node.gl/workflows/tests%20Mac/badge.svg)
![tests MinGW](https://github.com/gopro/gopro-lib-node.gl/workflows/tests%20MinGW/badge.svg)
![tests MSVC](https://github.com/gopro/gopro-lib-node.gl/workflows/tests%20MSVC/badge.svg)
[![coverage](https://codecov.io/gh/gopro/gopro-lib-node.gl/branch/master/graph/badge.svg)](https://codecov.io/gh/gopro/gopro-lib-node.gl)

[gopro]: https://gopro.com/


## 📜 License

`node.gl` is licensed under the Apache License, Version 2.0. Read the
[LICENSE][license] and [NOTICE][notice] files for details.

[license]: /LICENSE
[notice]: /NOTICE

## 👤📚 Users documentation

### 🛠 Tutorial

- [Starter tutorial][usr-tuto-start]

### 💡 How-to guides

- [Installation][usr-howto-install]
- [Using the C API][usr-howto-c-api]

### ⚙️ Discussions and explanations

- [node.gl standalone build environment][usr-expl-ngl-env]
- [Shaders][usr-expl-shaders]
- [Media (video) time remapping][usr-expl-time-remap]
- [Graphic configuration (blending, inheritance, ...)][usr-expl-graphicconfig]
- [How the Noise node works][usr-expl-noise]

### 🗜 Reference documentation

- [libnodegl][usr-ref-libnodegl]
- [pynodegl][usr-ref-pynodegl]
- [pynodegl-utils][usr-ref-pynodegl-utils]
- [ngl-tools][usr-ref-ngl-tools]
- [Eval][usr-ref-eval]


## 👷📚 Developers documentation

### 🛠 Tutorial

- [Writing a new node][dev-tuto-write-new-node]

### 💡 How-to guides

- [Working with tests][dev-howto-tests]
- [Release process][dev-howto-release-process]

### ⚙️ Discussions and explanations

- [How the Python binding is created][dev-expl-pynodegl]
- [What happens in a draw call?][dev-expl-draw-call]
- [Technical choices][dev-expl-techchoices]
- [The maths behind the YCbCr to RGBA color matrix][dev-expl-colormatrix]
- [Tests categories][dev-expl-tests-categories]

### 🗜 Reference documentation

- [Developer guidelines][dev-ref-developers]
- [Project architecture and organization][dev-ref-archi]


[usr-tuto-start]:            /doc/usr/tuto/start.md
[usr-howto-install]:         /doc/usr/howto/installation.md
[usr-howto-c-api]:           /doc/usr/howto/c-api.md
[usr-expl-ngl-env]:          /doc/usr/expl/ngl-env.md
[usr-expl-shaders]:          /doc/usr/expl/shaders.md
[usr-expl-time-remap]:       /doc/usr/expl/media-time-remapping.md
[usr-expl-graphicconfig]:    /doc/usr/expl/graphicconfig.md
[usr-expl-noise]:            /doc/usr/expl/noise.md
[usr-ref-libnodegl]:         /libnodegl/doc/libnodegl.md
[usr-ref-pynodegl]:          /doc/usr/ref/pynodegl.md
[usr-ref-pynodegl-utils]:    /doc/usr/ref/pynodegl-utils.md
[usr-ref-ngl-tools]:         /doc/usr/ref/ngl-tools.md
[usr-ref-eval]:              /doc/usr/ref/eval.md

[dev-tuto-write-new-node]:   /doc/dev/tuto/write-new-node.md
[dev-howto-tests]:           /doc/dev/howto/tests.md
[dev-howto-release-process]: /doc/dev/howto/release-process.md
[dev-expl-pynodegl]:         /doc/dev/expl/pynodegl.md
[dev-expl-draw-call]:        /doc/dev/expl/draw-call.md
[dev-expl-techchoices]:      /doc/dev/expl/techchoices.md
[dev-expl-colormatrix]:      /doc/dev/expl/colormatrix.md
[dev-expl-tests-categories]: /doc/dev/expl/tests-categories.md
[dev-ref-developers]:        /doc/dev/ref/developers.md
[dev-ref-archi]:             /doc/dev/ref/architecture.md
