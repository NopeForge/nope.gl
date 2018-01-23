libnodegl
=========

## AnimatedBuffer*

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`keyframes` |  | [`NodeList`](#parameter-types) ([AnimKeyFrameBuffer](#animkeyframebuffer)) | key frame buffers to interpolate from | 


**Source**: [node_animatedbuffer.c](/libnodegl/node_animatedbuffer.c)

List of `AnimatedBuffer*` nodes:

- `AnimatedBufferFloat`
- `AnimatedBufferVec2`
- `AnimatedBufferVec3`
- `AnimatedBufferVec4`

## AnimatedFloat

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`keyframes` |  | [`NodeList`](#parameter-types) ([AnimKeyFrameFloat](#animkeyframefloat)) | float key frames to interpolate from | 


**Source**: [node_animation.c](/libnodegl/node_animation.c)


## AnimatedVec2

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`keyframes` |  | [`NodeList`](#parameter-types) ([AnimKeyFrameVec2](#animkeyframevec2)) | vec2 key frames to interpolate from | 


**Source**: [node_animation.c](/libnodegl/node_animation.c)


## AnimatedVec3

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`keyframes` |  | [`NodeList`](#parameter-types) ([AnimKeyFrameVec3](#animkeyframevec3)) | vec3 key frames to interpolate from | 


**Source**: [node_animation.c](/libnodegl/node_animation.c)


## AnimatedVec4

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`keyframes` |  | [`NodeList`](#parameter-types) ([AnimKeyFrameVec4](#animkeyframevec4)) | vec4 key frames to interpolate from | 


**Source**: [node_animation.c](/libnodegl/node_animation.c)


## AnimKeyFrameFloat

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`time` | ✓ | [`double`](#parameter-types) | the time key point in seconds | `0`
`value` | ✓ | [`double`](#parameter-types) | the value at time `time` | `0`
`easing` |  | [`string`](#parameter-types) | a string identifying the interpolation | 
`easing_args` |  | [`doubleList`](#parameter-types) | a list of arguments some easings may use | 


**Source**: [node_animkeyframe.c](/libnodegl/node_animkeyframe.c)


## AnimKeyFrameVec2

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`time` | ✓ | [`double`](#parameter-types) | the time key point in seconds | `0`
`value` | ✓ | [`vec2`](#parameter-types) | the value at time `time` | (`0`,`0`)
`easing` |  | [`string`](#parameter-types) | a string identifying the interpolation | 
`easing_args` |  | [`doubleList`](#parameter-types) | a list of arguments some easings may use | 


**Source**: [node_animkeyframe.c](/libnodegl/node_animkeyframe.c)


## AnimKeyFrameVec3

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`time` | ✓ | [`double`](#parameter-types) | the time key point in seconds | `0`
`value` | ✓ | [`vec3`](#parameter-types) | the value at time `time` | (`0`,`0`,`0`)
`easing` |  | [`string`](#parameter-types) | a string identifying the interpolation | 
`easing_args` |  | [`doubleList`](#parameter-types) | a list of arguments some easings may use | 


**Source**: [node_animkeyframe.c](/libnodegl/node_animkeyframe.c)


## AnimKeyFrameVec4

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`time` | ✓ | [`double`](#parameter-types) | the time key point in seconds | `0`
`value` | ✓ | [`vec4`](#parameter-types) | the value at time `time` | (`0`,`0`,`0`,`0`)
`easing` |  | [`string`](#parameter-types) | a string identifying the interpolation | 
`easing_args` |  | [`doubleList`](#parameter-types) | a list of arguments some easings may use | 


**Source**: [node_animkeyframe.c](/libnodegl/node_animkeyframe.c)


## AnimKeyFrameBuffer

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`time` | ✓ | [`double`](#parameter-types) | the time key point in seconds | `0`
`data` |  | [`data`](#parameter-types) | the data at time `time` | 
`easing` |  | [`string`](#parameter-types) | a string identifying the interpolation | 
`easing_args` |  | [`doubleList`](#parameter-types) | a list of arguments some easings may use | 


**Source**: [node_animkeyframe.c](/libnodegl/node_animkeyframe.c)


## Buffer*

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`count` |  | [`int`](#parameter-types) | number of elements | `0`
`data` |  | [`data`](#parameter-types) | buffer of `count` elements | 
`stride` |  | [`int`](#parameter-types) | stride of 1 element, in bytes | `0`
`target` |  | [`buffer_target`](#buffer_target-choices) | target to which the buffer will be bound | `array_buffer`
`usage` |  | [`buffer_usage`](#buffer_usage-choices) | buffer usage hint | `static_draw`


**Source**: [node_buffer.c](/libnodegl/node_buffer.c)

List of `Buffer*` nodes:

- `BufferByte`
- `BufferBVec2`
- `BufferBVec3`
- `BufferBVec4`
- `BufferInt`
- `BufferIVec2`
- `BufferIVec3`
- `BufferIVec4`
- `BufferShort`
- `BufferSVec2`
- `BufferSVec3`
- `BufferSVec4`
- `BufferUByte`
- `BufferUBVec2`
- `BufferUBVec3`
- `BufferUBVec4`
- `BufferUInt`
- `BufferUIVec2`
- `BufferUIVec3`
- `BufferUIVec4`
- `BufferUShort`
- `BufferUSVec2`
- `BufferUSVec3`
- `BufferUSVec4`
- `BufferFloat`
- `BufferVec2`
- `BufferVec3`
- `BufferVec4`

## Camera

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | [`Node`](#parameter-types) | scene to observe through the lens of the camera | 
`eye` |  | [`vec3`](#parameter-types) | eye position | (`0`,`0`,`0`)
`center` |  | [`vec3`](#parameter-types) | center position | (`0`,`0`,`-1`)
`up` |  | [`vec3`](#parameter-types) | up vector | (`0`,`1`,`0`)
`perspective` |  | [`vec4`](#parameter-types) | the 4 following values: *fov*, *aspect*, *near clipping plane*, *far clipping plane* | (`0`,`0`,`0`,`0`)
`eye_transform` |  | [`Node`](#parameter-types) ([Rotate](#rotate), [Transform](#transform), [Translate](#translate), [Scale](#scale)) | `eye` transformation chain | 
`center_transform` |  | [`Node`](#parameter-types) ([Rotate](#rotate), [Transform](#transform), [Translate](#translate), [Scale](#scale)) | `center` transformation chain | 
`up_transform` |  | [`Node`](#parameter-types) ([Rotate](#rotate), [Transform](#transform), [Translate](#translate), [Scale](#scale)) | `up` transformation chain | 
`fov_anim` |  | [`Node`](#parameter-types) ([AnimatedFloat](#animatedfloat)) | field of view animation (first field of `perspective`) | 
`pipe_fd` |  | [`int`](#parameter-types) | pipe file descriptor where the rendered raw RGBA buffer is written | `0`
`pipe_width` |  | [`int`](#parameter-types) | width (in pixels) of the raw image buffer when using `pipe_fd` | `0`
`pipe_height` |  | [`int`](#parameter-types) | height (in pixels) of the raw image buffer when using `pipe_fd` | `0`


**Source**: [node_camera.c](/libnodegl/node_camera.c)


## Circle

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`radius` |  | [`double`](#parameter-types) | circle radius | `1`
`npoints` |  | [`int`](#parameter-types) | number of points | `16`


**Source**: [node_circle.c](/libnodegl/node_circle.c)


## Compute

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`nb_group_x` | ✓ | [`int`](#parameter-types) |  | `0`
`nb_group_y` | ✓ | [`int`](#parameter-types) |  | `0`
`nb_group_z` | ✓ | [`int`](#parameter-types) |  | `0`
`program` | ✓ | [`Node`](#parameter-types) ([ComputeProgram](#computeprogram)) |  | 
`textures` |  | [`NodeDict`](#parameter-types) ([Texture2D](#texture2d)) |  | 
`uniforms` |  | [`NodeDict`](#parameter-types) ([UniformFloat](#uniformfloat), [UniformVec2](#uniformvec2), [UniformVec3](#uniformvec3), [UniformVec4](#uniformvec4), [UniformInt](#uniformint), [UniformMat4](#uniformmat4)) |  | 
`buffers` |  | [`NodeDict`](#parameter-types) ([BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec3](#buffer), [BufferVec4](#buffer), [BufferInt](#buffer), [BufferIVec2](#buffer), [BufferIVec3](#buffer), [BufferIVec4](#buffer), [BufferUInt](#buffer), [BufferUIVec2](#buffer), [BufferUIVec3](#buffer), [BufferUIVec4](#buffer)) |  | 


**Source**: [node_compute.c](/libnodegl/node_compute.c)


## ComputeProgram

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`compute` | ✓ | [`string`](#parameter-types) |  | 


**Source**: [node_computeprogram.c](/libnodegl/node_computeprogram.c)


## FPS

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | [`Node`](#parameter-types) | scene to benchmark | 
`measure_update` |  | [`int`](#parameter-types) | window size of update measures | `60`
`measure_draw` |  | [`int`](#parameter-types) | window size of draw measures | `60`
`create_databuf` |  | [`bool`](#parameter-types) | create a data buffer to be used as source for a texture | `0`


**Source**: [node_fps.c](/libnodegl/node_fps.c)


## Geometry

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`vertices` | ✓ | [`Node`](#parameter-types) ([BufferVec3](#buffer), [AnimatedBufferVec3](#animatedbuffer)) |  | 
`uvcoords` |  | [`Node`](#parameter-types) ([BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec3](#buffer), [AnimatedBufferFloat](#animatedbuffer), [AnimatedBufferVec2](#animatedbuffer), [AnimatedBufferVec3](#animatedbuffer)) |  | 
`normals` |  | [`Node`](#parameter-types) ([BufferVec3](#buffer), [AnimatedBufferVec3](#animatedbuffer)) |  | 
`indices` |  | [`Node`](#parameter-types) ([BufferUByte](#buffer), [BufferUInt](#buffer), [BufferUShort](#buffer)) |  | 
`draw_mode` |  | [`draw_mode`](#draw_mode-choices) |  | `triangles`


**Source**: [node_geometry.c](/libnodegl/node_geometry.c)


## GraphicConfig

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | [`Node`](#parameter-types) |  | 
`blend` |  | [`bool`](#parameter-types) |  | `unset`
`blend_src_factor` |  | [`blend_factor`](#blend_factor-choices) |  | `unset`
`blend_dst_factor` |  | [`blend_factor`](#blend_factor-choices) |  | `unset`
`blend_src_factor_a` |  | [`blend_factor`](#blend_factor-choices) |  | `unset`
`blend_dst_factor_a` |  | [`blend_factor`](#blend_factor-choices) |  | `unset`
`blend_op` |  | [`blend_operation`](#blend_operation-choices) |  | `unset`
`blend_op_a` |  | [`blend_operation`](#blend_operation-choices) |  | `unset`
`color_write_mask` |  | [`component`](#component-choices) |  | `unset`
`depth_test` |  | [`bool`](#parameter-types) |  | `unset`
`depth_write_mask` |  | [`bool`](#parameter-types) |  | `unset`
`depth_func` |  | [`function`](#function-choices) | passes if `<function>(depth, stored_depth)` | `unset`
`stencil_test` |  | [`bool`](#parameter-types) |  | `unset`
`stencil_write_mask` |  | [`int`](#parameter-types) |  | `-1`
`stencil_func` |  | [`function`](#function-choices) | passes if `<function>(stencil_ref & stencil_read_mask, stencil & stencil_read_mask)` | `unset`
`stencil_ref` |  | [`int`](#parameter-types) |  | `-1`
`stencil_read_mask` |  | [`int`](#parameter-types) |  | `-1`
`stencil_fail` |  | [`stencil_operation`](#stencil_operation-choices) |  | `unset`
`stencil_depth_fail` |  | [`stencil_operation`](#stencil_operation-choices) |  | `unset`
`stencil_depth_pass` |  | [`stencil_operation`](#stencil_operation-choices) |  | `unset`


**Source**: [node_graphicconfig.c](/libnodegl/node_graphicconfig.c)


## Group

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`children` |  | [`NodeList`](#parameter-types) | a set of scenes | 


**Source**: [node_group.c](/libnodegl/node_group.c)


## Identity

**Source**: [node_identity.c](/libnodegl/node_identity.c)


## Media

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`filename` | ✓ | [`string`](#parameter-types) | path to input media file | 
`sxplayer_min_level` |  | [`string`](#parameter-types) | sxplayer min logging level | 
`time_anim` |  | [`Node`](#parameter-types) ([AnimatedFloat](#animatedfloat)) | time remapping animation (must use a `linear` interpolation) | 
`audio_tex` |  | [`bool`](#parameter-types) | load the audio and expose it as a stereo waves and frequencies buffer | `0`
`max_nb_packets` |  | [`int`](#parameter-types) | maximum number of packets in sxplayer demuxing queue | `1`
`max_nb_frames` |  | [`int`](#parameter-types) | maximum number of frames in sxplayer decoding queue | `1`
`max_nb_sink` |  | [`int`](#parameter-types) | maximum number of frames in sxplayer filtering queue | `1`
`max_pixels` |  | [`int`](#parameter-types) | maximum number of pixels per frame | `0`


**Source**: [node_media.c](/libnodegl/node_media.c)


## Program

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`vertex` |  | [`string`](#parameter-types) | vertex shader | 
`fragment` |  | [`string`](#parameter-types) | fragment shader | 


**Source**: [node_program.c](/libnodegl/node_program.c)


## Quad

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`corner` |  | [`vec3`](#parameter-types) |  | (`-0.5`,`-0.5`,`0`)
`width` |  | [`vec3`](#parameter-types) |  | (`1`,`0`,`0`)
`height` |  | [`vec3`](#parameter-types) |  | (`0`,`1`,`0`)
`uv_corner` |  | [`vec2`](#parameter-types) |  | (`0`,`0`)
`uv_width` |  | [`vec2`](#parameter-types) |  | (`1`,`0`)
`uv_height` |  | [`vec2`](#parameter-types) |  | (`0`,`1`)


**Source**: [node_quad.c](/libnodegl/node_quad.c)


## Render

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`geometry` | ✓ | [`Node`](#parameter-types) ([Circle](#circle), [Geometry](#geometry), [Quad](#quad), [Triangle](#triangle)) |  | 
`program` |  | [`Node`](#parameter-types) ([Program](#program)) |  | 
`textures` |  | [`NodeDict`](#parameter-types) ([Texture2D](#texture2d), [Texture3D](#texture3d)) |  | 
`uniforms` |  | [`NodeDict`](#parameter-types) ([UniformFloat](#uniformfloat), [UniformVec2](#uniformvec2), [UniformVec3](#uniformvec3), [UniformVec4](#uniformvec4), [UniformInt](#uniformint), [UniformMat4](#uniformmat4)) |  | 
`attributes` |  | [`NodeDict`](#parameter-types) ([BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec3](#buffer), [BufferVec4](#buffer)) |  | 
`buffers` |  | [`NodeDict`](#parameter-types) ([BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec3](#buffer), [BufferVec4](#buffer), [BufferInt](#buffer), [BufferIVec2](#buffer), [BufferIVec3](#buffer), [BufferIVec4](#buffer), [BufferUInt](#buffer), [BufferUIVec2](#buffer), [BufferUIVec3](#buffer), [BufferUIVec4](#buffer)) |  | 


**Source**: [node_render.c](/libnodegl/node_render.c)


## RenderToTexture

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | [`Node`](#parameter-types) |  | 
`color_texture` | ✓ | [`Node`](#parameter-types) ([Texture2D](#texture2d)) |  | 
`depth_texture` |  | [`Node`](#parameter-types) ([Texture2D](#texture2d)) |  | 


**Source**: [node_rtt.c](/libnodegl/node_rtt.c)


## Rotate

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | [`Node`](#parameter-types) | scene to rotate | 
`angle` |  | [`double`](#parameter-types) | rotation angle in degrees | `0`
`axis` |  | [`vec3`](#parameter-types) | rotation axis | (`0`,`0`,`1`)
`anchor` |  | [`vec3`](#parameter-types) | vector to the center point of the rotation | (`0`,`0`,`0`)
`anim` |  | [`Node`](#parameter-types) ([AnimatedFloat](#animatedfloat)) | `angle` animation | 


**Source**: [node_rotate.c](/libnodegl/node_rotate.c)


## Scale

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | [`Node`](#parameter-types) | scene to scale | 
`factors` |  | [`vec3`](#parameter-types) | scaling factors (how much to scale on each axis) | (`0`,`0`,`0`)
`anchor` |  | [`vec3`](#parameter-types) | vector to the center point of the scale | (`0`,`0`,`0`)
`anim` |  | [`Node`](#parameter-types) ([AnimatedVec3](#animatedvec3)) | `factors` animation | 


**Source**: [node_scale.c](/libnodegl/node_scale.c)


## Texture2D

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`format` |  | [`format`](#format-choices) |  | `rgba`
`internal_format` |  | [`internal_format`](#internal_format-choices) |  | `rgba`
`type` |  | [`type`](#type-choices) |  | `unsigned_byte`
`width` |  | [`int`](#parameter-types) |  | `0`
`height` |  | [`int`](#parameter-types) |  | `0`
`min_filter` |  | [`min_filter`](#min_filter-choices) |  | `nearest`
`mag_filter` |  | [`mag_filter`](#mag_filter-choices) |  | `nearest`
`wrap_s` |  | [`wrap`](#wrap-choices) |  | `clamp_to_edge`
`wrap_t` |  | [`wrap`](#wrap-choices) |  | `clamp_to_edge`
`data_src` |  | [`Node`](#parameter-types) ([Media](#media), [FPS](#fps), [AnimatedBufferFloat](#animatedbuffer), [AnimatedBufferVec2](#animatedbuffer), [AnimatedBufferVec3](#animatedbuffer), [AnimatedBufferVec4](#animatedbuffer), [BufferByte](#buffer), [BufferBVec2](#buffer), [BufferBVec3](#buffer), [BufferBVec4](#buffer), [BufferInt](#buffer), [BufferIVec2](#buffer), [BufferIVec3](#buffer), [BufferIVec4](#buffer), [BufferShort](#buffer), [BufferSVec2](#buffer), [BufferSVec3](#buffer), [BufferSVec4](#buffer), [BufferUByte](#buffer), [BufferUBVec2](#buffer), [BufferUBVec3](#buffer), [BufferUBVec4](#buffer), [BufferUInt](#buffer), [BufferUIVec2](#buffer), [BufferUIVec3](#buffer), [BufferUIVec4](#buffer), [BufferUShort](#buffer), [BufferUSVec2](#buffer), [BufferUSVec3](#buffer), [BufferUSVec4](#buffer), [BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec3](#buffer), [BufferVec4](#buffer)) |  | 
`access` |  | [`access`](#access-choices) |  | `read_write`
`direct_rendering` |  | [`bool`](#parameter-types) |  | `unset`
`immutable` |  | [`bool`](#parameter-types) |  | `0`


**Source**: [node_texture.c](/libnodegl/node_texture.c)


## Texture3D

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`format` |  | [`format`](#format-choices) |  | `rgba`
`internal_format` |  | [`internal_format`](#internal_format-choices) |  | `rgba`
`type` |  | [`type`](#type-choices) |  | `unsigned_byte`
`width` |  | [`int`](#parameter-types) |  | `0`
`height` |  | [`int`](#parameter-types) |  | `0`
`depth` |  | [`int`](#parameter-types) |  | `0`
`min_filter` |  | [`min_filter`](#min_filter-choices) |  | `nearest`
`mag_filter` |  | [`mag_filter`](#mag_filter-choices) |  | `nearest`
`wrap_s` |  | [`wrap`](#wrap-choices) |  | `clamp_to_edge`
`wrap_t` |  | [`wrap`](#wrap-choices) |  | `clamp_to_edge`
`wrap_r` |  | [`wrap`](#wrap-choices) |  | `clamp_to_edge`
`data_src` |  | [`Node`](#parameter-types) ([AnimatedBufferFloat](#animatedbuffer), [AnimatedBufferVec2](#animatedbuffer), [AnimatedBufferVec3](#animatedbuffer), [AnimatedBufferVec4](#animatedbuffer), [BufferByte](#buffer), [BufferBVec2](#buffer), [BufferBVec3](#buffer), [BufferBVec4](#buffer), [BufferInt](#buffer), [BufferIVec2](#buffer), [BufferIVec3](#buffer), [BufferIVec4](#buffer), [BufferShort](#buffer), [BufferSVec2](#buffer), [BufferSVec3](#buffer), [BufferSVec4](#buffer), [BufferUByte](#buffer), [BufferUBVec2](#buffer), [BufferUBVec3](#buffer), [BufferUBVec4](#buffer), [BufferUInt](#buffer), [BufferUIVec2](#buffer), [BufferUIVec3](#buffer), [BufferUIVec4](#buffer), [BufferUShort](#buffer), [BufferUSVec2](#buffer), [BufferUSVec3](#buffer), [BufferUSVec4](#buffer), [BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec3](#buffer), [BufferVec4](#buffer)) |  | 
`access` |  | [`access`](#access-choices) |  | `read_write`
`immutable` |  | [`bool`](#parameter-types) |  | `0`


**Source**: [node_texture.c](/libnodegl/node_texture.c)


## TimeRangeFilter

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | [`Node`](#parameter-types) | time filtered scene | 
`ranges` |  | [`NodeList`](#parameter-types) ([TimeRangeModeOnce](#timerangemodeonce), [TimeRangeModeNoop](#timerangemodenoop), [TimeRangeModeCont](#timerangemodecont)) | key frame time filtering events | 
`prefetch_time` |  | [`double`](#parameter-types) | `child` is prefetched `prefetch_time` seconds in advance | `1`
`max_idle_time` |  | [`double`](#parameter-types) | `child` will not be released if it is required in the next incoming `max_idle_time` seconds | `4`


**Source**: [node_timerangefilter.c](/libnodegl/node_timerangefilter.c)


## TimeRangeModeCont

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`start_time` | ✓ | [`double`](#parameter-types) | starting time for the scene to be drawn | `0`


**Source**: [node_timerangemodes.c](/libnodegl/node_timerangemodes.c)


## TimeRangeModeNoop

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`start_time` | ✓ | [`double`](#parameter-types) | starting time for the scene to stop being drawn | `0`


**Source**: [node_timerangemodes.c](/libnodegl/node_timerangemodes.c)


## TimeRangeModeOnce

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`start_time` | ✓ | [`double`](#parameter-types) | starting time for the scene to be drawn once | `0`
`render_time` | ✓ | [`double`](#parameter-types) | chosen time to draw | `0`


**Source**: [node_timerangemodes.c](/libnodegl/node_timerangemodes.c)


## Transform

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | [`Node`](#parameter-types) | scene to apply the transform to | 
`matrix` |  | [`mat4`](#parameter-types) | transformation matrix | 


**Source**: [node_transform.c](/libnodegl/node_transform.c)


## Translate

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | [`Node`](#parameter-types) | scene to translate | 
`vector` |  | [`vec3`](#parameter-types) | translation vector | (`0`,`0`,`0`)
`anim` |  | [`Node`](#parameter-types) ([AnimatedVec3](#animatedvec3)) | `vector` animation | 


**Source**: [node_translate.c](/libnodegl/node_translate.c)


## Triangle

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`edge0` | ✓ | [`vec3`](#parameter-types) |  | (`0`,`0`,`0`)
`edge1` | ✓ | [`vec3`](#parameter-types) |  | (`0`,`0`,`0`)
`edge2` | ✓ | [`vec3`](#parameter-types) |  | (`0`,`0`,`0`)
`uv_edge0` |  | [`vec2`](#parameter-types) |  | (`0`,`0`)
`uv_edge1` |  | [`vec2`](#parameter-types) |  | (`0`,`1`)
`uv_edge2` |  | [`vec2`](#parameter-types) |  | (`1`,`1`)


**Source**: [node_triangle.c](/libnodegl/node_triangle.c)


## UniformInt

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`value` |  | [`int`](#parameter-types) |  | `0`


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)


## UniformMat4

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`value` |  | [`mat4`](#parameter-types) |  | 
`transform` |  | [`Node`](#parameter-types) ([Rotate](#rotate), [Transform](#transform), [Translate](#translate), [Scale](#scale)) |  | 


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)


## UniformFloat

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`value` |  | [`double`](#parameter-types) |  | `0`
`anim` |  | [`Node`](#parameter-types) ([AnimatedFloat](#animatedfloat)) |  | 


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)


## UniformVec2

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`value` |  | [`vec2`](#parameter-types) |  | (`0`,`0`)
`anim` |  | [`Node`](#parameter-types) ([AnimatedVec2](#animatedvec2)) |  | 


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)


## UniformVec3

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`value` |  | [`vec3`](#parameter-types) |  | (`0`,`0`,`0`)
`anim` |  | [`Node`](#parameter-types) ([AnimatedVec3](#animatedvec3)) |  | 


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)


## UniformVec4

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`value` |  | [`vec4`](#parameter-types) |  | (`0`,`0`,`0`,`0`)
`anim` |  | [`Node`](#parameter-types) ([AnimatedVec4](#animatedvec4)) |  | 


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)

Parameter types
===============

Type | Description
---- | -----------
`int` | Integer
`bool` | Boolean (map to `int` in C)
`i64` | 64-bit integer
`double` | Double-precision float
`string` | String
`data` | Agnostic data buffer
`vec2` | 2 single-precision floats
`vec3` | 3 single-precision floats
`vec4` | 4 single-precision floats
`mat4` | 4x4 single-precision floats
`Node` | node.gl Node
`NodeList` | List of node.gl Node
`doubleList` | List of double-precision floats
`NodeDict` | Dictionary mapping arbitrary string identifiers to node.gl Nodes
`select` | Selection of one constant (expressed as a string)
`flags` | Combination of constants (expressed as strings), using `+` as separator. Can be empty for none.

Constants for choices parameters
================================

## buffer_target choices

Constant | Description
-------- | -----------
`array_buffer` | vertex attributes
`element_array_buffer` | vertex array indices
`shader_storage_buffer` | read-write storage for shaders

## buffer_usage choices

Constant | Description
-------- | -----------
`stream_draw` | modified once by the application and used at most a few times as a source for drawing
`stream_read` | modified once by reading data from the graphic pipeline and used at most a few times to return the data to the application
`stream_copy` | modified once by reading data from the graphic pipeline and used at most a few times as a source for drawing
`static_draw` | modified once by the application and used many times as a source for drawing
`static_read` | modified once by reading data from the graphic pipeline and used many times to return the data to the application
`static_copy` | modified once by reading data from the graphic pipeline and used at most a few times a source for drawing
`dynamic_draw` | modified repeatedly by the application and used many times as a source for drawing
`dynamic_read` | modified repeatedly by reading data from the graphic pipeline and used many times to return data to the application
`dynamic_copy` | modified repeatedly by reading data from the graphic pipeline and used many times as a source for drawing

## draw_mode choices

Constant | Description
-------- | -----------
`points` | points
`line_strip` | line strip
`line_loop` | line loop
`lines` | lines
`triangle_strip` | triangle strip
`triangle_fan` | triangle fan
`triangles` | triangles

## blend_factor choices

Constant | Description
-------- | -----------
`unset` | unset
`zero` | `0`
`one` | `1`
`src_color` | `src_color`
`one_minus_src_color` | `1 - src_color`
`dst_color` | `dst_color`
`one_minus_dst_color` | `1 - dst_color`
`src_alpha` | `src_alpha`
`one_minus_src_alpha` | `1 - src_alpha`
`dst_alpha` | `dst_alpha`
`one_minus_dst_alpha` | `1 - dst_alpha`

## blend_operation choices

Constant | Description
-------- | -----------
`unset` | unset
`add` | `src + dst`
`sub` | `src - dst`
`revsub` | `dst - src`
`min` | `min(src, dst)`
`max` | `max(src, dst)`

## component choices

Constant | Description
-------- | -----------
`r` | red
`g` | green
`b` | blue
`a` | alpha

## function choices

Constant | Description
-------- | -----------
`unset` | unset
`never` | `f(a,b) = 0`
`less` | `f(a,b) = a < b`
`equal` | `f(a,b) = a == b`
`lequal` | `f(a,b) = a ≤ b`
`greater` | `f(a,b) = a > b`
`notequal` | `f(a,b) = a ≠ b`
`gequal` | `f(a,b) = a ≥ b`
`always` | `f(a,b) = 1`

## stencil_operation choices

Constant | Description
-------- | -----------
`unset` | unset
`keep` | keeps the current value
`zero` | sets the stencil buffer value to 0
`replace` | sets the stencil buffer value to ref, as specified by the stencil function
`incr` | increments the current stencil buffer value and clamps it
`incr_wrap` | increments the current stencil buffer value and wraps it
`decr` | decrements the current stencil buffer value and clamps it
`decr_wrap` | decrements the current stencil buffer value and wraps it
`decr_invert` | bitwise inverts the current stencil buffer value

## format choices

Constant | Description
-------- | -----------
`red` | red
`red_integer` | red integer
`rg` | rg
`rg_integer` | rg integer
`rgb` | rgb
`rgb_integer` | rgb integer
`rgba` | rgba
`rgba_integer` | rgba integer
`bgra` | bgra
`depth_component` | depth component
`depth_stencil` | depth stencil
`alpha` | alpha (OpenGLES only)
`luminance` | luminance (OpenGLES only)
`luminance_alpha` | luminance alpha (OpenGLES only)

## internal_format choices

Constant | Description
-------- | -----------
`red` | red
`rg` | rg
`rgb` | rgb
`rgba` | rgba
`depth_component` | depth component
`depth_stencil` | depth stencil
`alpha` | alpha (OpenGLES only)
`luminance` | luminance (OpenGLES only)
`luminance_alpha` | luminance alpha (OpenGLES only)

## type choices

Constant | Description
-------- | -----------
`byte` | byte
`unsigned_byte` | unsigned byte
`short` | short
`unsigned_short` | unsigned short
`int` | integer
`unsigned_int` | unsigned integer
`half_float` | half float
`float` | float

## min_filter choices

Constant | Description
-------- | -----------
`nearest` | nearest filtering
`linear` | linear filtering
`nearest_mipmap_nearest` | nearest filtering, nearest mipmap filtering
`linear_mipmap_nearest` | linear filtering, nearest mipmap filtering
`nearest_mipmap_linear` | nearest filtering, linear mipmap filtering
`linear_mipmap_linear` | linear filtering, linear mipmap filtering

## mag_filter choices

Constant | Description
-------- | -----------
`nearest` | nearest filtering
`linear` | linear filtering

## wrap choices

Constant | Description
-------- | -----------
`clamp_to_edge` | clamp to edge wrapping
`mirrored_repeat` | mirrored repeat wrapping
`repeat` | repeat pattern wrapping

## access choices

Constant | Description
-------- | -----------
`read_only` | read only
`write_only` | write only
`read_write` | read-write
