libnodegl
=========
## AnimatedBuffer*

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`keyframes` |  | `NodeList` ([AnimKeyFrameBuffer](#animkeyframebuffer)) | key frame buffers to interpolate from | 


**Source**: [node_animatedbuffer.c](/libnodegl/node_animatedbuffer.c)

List of `AnimatedBuffer*` nodes:

- `AnimatedBufferFloat`
- `AnimatedBufferVec2`
- `AnimatedBufferVec3`
- `AnimatedBufferVec4`
## AnimatedFloat

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`keyframes` |  | `NodeList` ([AnimKeyFrameFloat](#animkeyframefloat)) | float key frames to interpolate from | 


**Source**: [node_animation.c](/libnodegl/node_animation.c)

## AnimatedVec2

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`keyframes` |  | `NodeList` ([AnimKeyFrameVec2](#animkeyframevec2)) | vec2 key frames to interpolate from | 


**Source**: [node_animation.c](/libnodegl/node_animation.c)

## AnimatedVec3

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`keyframes` |  | `NodeList` ([AnimKeyFrameVec3](#animkeyframevec3)) | vec3 key frames to interpolate from | 


**Source**: [node_animation.c](/libnodegl/node_animation.c)

## AnimatedVec4

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`keyframes` |  | `NodeList` ([AnimKeyFrameVec4](#animkeyframevec4)) | vec4 key frames to interpolate from | 


**Source**: [node_animation.c](/libnodegl/node_animation.c)

## AnimKeyFrameFloat

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`time` | ✓ | `double` | the time key point in seconds | `0`
`value` | ✓ | `double` | the value at time `time` | `0`
`easing` |  | `string` | a string identifying the interpolation | 
`easing_args` |  | `doubleList` | a list of arguments some easings may use | 


**Source**: [node_animkeyframe.c](/libnodegl/node_animkeyframe.c)

## AnimKeyFrameVec2

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`time` | ✓ | `double` | the time key point in seconds | `0`
`value` | ✓ | `vec2` | the value at time `time` | (`0`,`0`)
`easing` |  | `string` | a string identifying the interpolation | 
`easing_args` |  | `doubleList` | a list of arguments some easings may use | 


**Source**: [node_animkeyframe.c](/libnodegl/node_animkeyframe.c)

## AnimKeyFrameVec3

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`time` | ✓ | `double` | the time key point in seconds | `0`
`value` | ✓ | `vec3` | the value at time `time` | (`0`,`0`,`0`)
`easing` |  | `string` | a string identifying the interpolation | 
`easing_args` |  | `doubleList` | a list of arguments some easings may use | 


**Source**: [node_animkeyframe.c](/libnodegl/node_animkeyframe.c)

## AnimKeyFrameVec4

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`time` | ✓ | `double` | the time key point in seconds | `0`
`value` | ✓ | `vec4` | the value at time `time` | (`0`,`0`,`0`,`0`)
`easing` |  | `string` | a string identifying the interpolation | 
`easing_args` |  | `doubleList` | a list of arguments some easings may use | 


**Source**: [node_animkeyframe.c](/libnodegl/node_animkeyframe.c)

## AnimKeyFrameBuffer

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`time` | ✓ | `double` | the time key point in seconds | `0`
`data` |  | `data` | the data at time `time` | 
`easing` |  | `string` | a string identifying the interpolation | 
`easing_args` |  | `doubleList` | a list of arguments some easings may use | 


**Source**: [node_animkeyframe.c](/libnodegl/node_animkeyframe.c)

## Buffer*

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`count` |  | `int` | number of elements | `0`
`data` |  | `data` | buffer of `count` elements | 
`stride` |  | `int` | stride of 1 element, in bytes | `0`
`target` |  | `int` |  | `34962`
`usage` |  | `int` |  | `35044`


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
`child` | ✓ | `Node` | scene to observe through the lens of the camera | 
`eye` |  | `vec3` | eye position | (`0`,`0`,`0`)
`center` |  | `vec3` | center position | (`0`,`0`,`-1`)
`up` |  | `vec3` | up vector | (`0`,`1`,`0`)
`perspective` |  | `vec4` | the 4 following values: *fov*, *aspect*, *near clipping plane*, *far clipping plane* | (`0`,`0`,`0`,`0`)
`eye_transform` |  | `Node` ([Rotate](#rotate), [Translate](#translate), [Scale](#scale)) | `eye` transformation chain | 
`center_transform` |  | `Node` ([Rotate](#rotate), [Translate](#translate), [Scale](#scale)) | `center` transformation chain | 
`up_transform` |  | `Node` ([Rotate](#rotate), [Translate](#translate), [Scale](#scale)) | `up` transformation chain | 
`fov_anim` |  | `Node` ([AnimatedFloat](#animatedfloat)) | field of view animation (first field of `perspective`) | 
`pipe_fd` |  | `int` | pipe file descriptor where the rendered raw RGBA buffer is written | `0`
`pipe_width` |  | `int` | width (in pixels) of the raw image buffer when using `pipe_fd` | `0`
`pipe_height` |  | `int` | height (in pixels) of the raw image buffer when using `pipe_fd` | `0`


**Source**: [node_camera.c](/libnodegl/node_camera.c)

## Circle

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`radius` |  | `double` | circle radius | `1`
`npoints` |  | `int` | number of points | `16`


**Source**: [node_circle.c](/libnodegl/node_circle.c)

## Compute

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`nb_group_x` | ✓ | `int` |  | `0`
`nb_group_y` | ✓ | `int` |  | `0`
`nb_group_z` | ✓ | `int` |  | `0`
`program` | ✓ | `Node` ([ComputeProgram](#computeprogram)) |  | 
`textures` |  | `NodeDict` ([Texture2D](#texture2d)) |  | 
`uniforms` |  | `NodeDict` ([UniformFloat](#uniformfloat), [UniformVec2](#uniformvec2), [UniformVec3](#uniformvec3), [UniformVec4](#uniformvec4), [UniformInt](#uniformint), [UniformMat4](#uniformmat4)) |  | 
`buffers` |  | `NodeDict` ([BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec3](#buffer), [BufferVec4](#buffer), [BufferInt](#buffer), [BufferIVec2](#buffer), [BufferIVec3](#buffer), [BufferIVec4](#buffer), [BufferUInt](#buffer), [BufferUIVec2](#buffer), [BufferUIVec3](#buffer), [BufferUIVec4](#buffer)) |  | 


**Source**: [node_compute.c](/libnodegl/node_compute.c)

## ComputeProgram

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`compute` | ✓ | `string` |  | 


**Source**: [node_computeprogram.c](/libnodegl/node_computeprogram.c)

## FPS

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | `Node` | scene to benchmark | 
`measure_update` |  | `int` | window size of update measures | `60`
`measure_draw` |  | `int` | window size of draw measures | `60`
`create_databuf` |  | `int` | create a data buffer to be used as source for a texture | `0`


**Source**: [node_fps.c](/libnodegl/node_fps.c)

## Geometry

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`vertices` | ✓ | `Node` ([BufferVec3](#buffer), [AnimatedBufferVec3](#animatedbuffer)) |  | 
`uvcoords` |  | `Node` ([BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec3](#buffer), [AnimatedBufferFloat](#animatedbuffer), [AnimatedBufferVec2](#animatedbuffer), [AnimatedBufferVec3](#animatedbuffer)) |  | 
`normals` |  | `Node` ([BufferVec3](#buffer), [AnimatedBufferVec3](#animatedbuffer)) |  | 
`indices` |  | `Node` ([BufferUByte](#buffer), [BufferUInt](#buffer), [BufferUShort](#buffer)) |  | 
`draw_mode` |  | `int` |  | `4`


**Source**: [node_geometry.c](/libnodegl/node_geometry.c)

## GraphicConfig

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | `Node` |  | 
`blend` |  | `Node` ([ConfigBlend](#configblend)) |  | 
`colormask` |  | `Node` ([ConfigColorMask](#configcolormask)) |  | 
`depth` |  | `Node` ([ConfigDepth](#configdepth)) |  | 
`polygonmode` |  | `Node` ([ConfigPolygonMode](#configpolygonmode)) |  | 
`stencil` |  | `Node` ([ConfigStencil](#configstencil)) |  | 


**Source**: [node_graphicconfig.c](/libnodegl/node_graphicconfig.c)

## ConfigBlend

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`enabled` | ✓ | `int` |  | `0`
`src_rgb` |  | `int` |  | `1`
`dst_rgb` |  | `int` |  | `0`
`src_alpha` |  | `int` |  | `1`
`dst_alpha` |  | `int` |  | `0`
`mode_rgb` |  | `int` |  | `32774`
`mode_alpha` |  | `int` |  | `32774`


**Source**: [node_configblend.c](/libnodegl/node_configblend.c)

## ConfigColorMask

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`enabled` | ✓ | `int` |  | `1`
`red` |  | `int` |  | `1`
`green` |  | `int` |  | `1`
`blue` |  | `int` |  | `1`
`alpha` |  | `int` |  | `1`


**Source**: [node_configcolormask.c](/libnodegl/node_configcolormask.c)

## ConfigPolygonMode

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`mode` | ✓ | `int` |  | `6914`


**Source**: [node_configpolygonmode.c](/libnodegl/node_configpolygonmode.c)

## ConfigDepth

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`enabled` | ✓ | `int` |  | `0`
`writemask` |  | `int` |  | `1`
`func` |  | `int` |  | `513`


**Source**: [node_configdepth.c](/libnodegl/node_configdepth.c)

## ConfigStencil

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`enabled` | ✓ | `int` |  | `0`
`writemask` |  | `int` |  | `255`
`func` |  | `int` |  | `519`
`func_ref` |  | `int` |  | `0`
`func_mask` |  | `int` |  | `255`
`op_sfail` |  | `int` |  | `7680`
`op_dpfail` |  | `int` |  | `7680`
`op_dppass` |  | `int` |  | `7680`


**Source**: [node_configstencil.c](/libnodegl/node_configstencil.c)

## Group

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`children` |  | `NodeList` | a set of scenes | 


**Source**: [node_group.c](/libnodegl/node_group.c)

## Identity

**Source**: [node_identity.c](/libnodegl/node_identity.c)

## Media

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`filename` | ✓ | `string` | path to input media file | 
`start` |  | `double` | update time offseting, updates before this time will do nothing | `0`
`initial_seek` |  | `double` | initial seek in the media | `0`
`sxplayer_min_level` |  | `string` | sxplayer min logging level | 
`time_anim` |  | `Node` ([AnimatedFloat](#animatedfloat)) | time remapping animation (must use a `linear` interpolation) | 
`audio_tex` |  | `int` | load the audio and expose it as a stereo waves and frequencies buffer | `0`
`max_nb_packets` |  | `int` | maximum number of packets in sxplayer demuxing queue | `1`
`max_nb_frames` |  | `int` | maximum number of frames in sxplayer decoding queue | `1`
`max_nb_sink` |  | `int` | maximum number of frames in sxplayer filtering queue | `1`
`max_pixels` |  | `int` | maximum number of pixels per frame | `0`


**Source**: [node_media.c](/libnodegl/node_media.c)

## Program

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`vertex` |  | `string` | vertex shader | 
`fragment` |  | `string` | fragment shader | 


**Source**: [node_program.c](/libnodegl/node_program.c)

## Quad

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`corner` |  | `vec3` |  | (`-0.5`,`-0.5`,`0`)
`width` |  | `vec3` |  | (`1`,`0`,`0`)
`height` |  | `vec3` |  | (`0`,`1`,`0`)
`uv_corner` |  | `vec2` |  | (`0`,`0`)
`uv_width` |  | `vec2` |  | (`1`,`0`)
`uv_height` |  | `vec2` |  | (`0`,`1`)


**Source**: [node_quad.c](/libnodegl/node_quad.c)

## Render

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`geometry` | ✓ | `Node` ([Circle](#circle), [Geometry](#geometry), [Quad](#quad), [Triangle](#triangle)) |  | 
`program` |  | `Node` ([Program](#program)) |  | 
`textures` |  | `NodeDict` ([Texture2D](#texture2d), [Texture3D](#texture3d)) |  | 
`uniforms` |  | `NodeDict` ([UniformFloat](#uniformfloat), [UniformVec2](#uniformvec2), [UniformVec3](#uniformvec3), [UniformVec4](#uniformvec4), [UniformInt](#uniformint), [UniformMat4](#uniformmat4)) |  | 
`attributes` |  | `NodeDict` ([BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec3](#buffer), [BufferVec4](#buffer)) |  | 
`buffers` |  | `NodeDict` ([BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec3](#buffer), [BufferVec4](#buffer), [BufferInt](#buffer), [BufferIVec2](#buffer), [BufferIVec3](#buffer), [BufferIVec4](#buffer), [BufferUInt](#buffer), [BufferUIVec2](#buffer), [BufferUIVec3](#buffer), [BufferUIVec4](#buffer)) |  | 


**Source**: [node_render.c](/libnodegl/node_render.c)

## RenderToTexture

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | `Node` |  | 
`color_texture` | ✓ | `Node` ([Texture2D](#texture2d)) |  | 
`depth_texture` |  | `Node` ([Texture2D](#texture2d)) |  | 


**Source**: [node_rtt.c](/libnodegl/node_rtt.c)

## Rotate

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | `Node` | scene to rotate | 
`angle` |  | `double` | rotation angle in degrees | `0`
`axis` |  | `vec3` | rotation axis | (`0`,`0`,`1`)
`anchor` |  | `vec3` | vector to the center point of the rotation | (`0`,`0`,`0`)
`anim` |  | `Node` ([AnimatedFloat](#animatedfloat)) | `angle` animation | 


**Source**: [node_rotate.c](/libnodegl/node_rotate.c)

## Scale

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | `Node` | scene to scale | 
`factors` |  | `vec3` | scaling factors (how much to scale on each axis) | (`0`,`0`,`0`)
`anchor` |  | `vec3` | vector to the center point of the scale | (`0`,`0`,`0`)
`anim` |  | `Node` ([AnimatedVec3](#animatedvec3)) | `factors` animation | 


**Source**: [node_scale.c](/libnodegl/node_scale.c)

## Texture2D

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`format` |  | `int` |  | `6408`
`internal_format` |  | `int` |  | `6408`
`type` |  | `int` |  | `5121`
`width` |  | `int` |  | `0`
`height` |  | `int` |  | `0`
`min_filter` |  | `int` |  | `9728`
`mag_filter` |  | `int` |  | `9728`
`wrap_s` |  | `int` |  | `33071`
`wrap_t` |  | `int` |  | `33071`
`data_src` |  | `Node` ([Media](#media), [FPS](#fps), [AnimatedBufferFloat](#animatedbuffer), [AnimatedBufferVec2](#animatedbuffer), [AnimatedBufferVec3](#animatedbuffer), [AnimatedBufferVec4](#animatedbuffer), [BufferByte](#buffer), [BufferBVec2](#buffer), [BufferBVec3](#buffer), [BufferBVec4](#buffer), [BufferInt](#buffer), [BufferIVec2](#buffer), [BufferIVec3](#buffer), [BufferIVec4](#buffer), [BufferShort](#buffer), [BufferSVec2](#buffer), [BufferSVec3](#buffer), [BufferSVec4](#buffer), [BufferUByte](#buffer), [BufferUBVec2](#buffer), [BufferUBVec3](#buffer), [BufferUBVec4](#buffer), [BufferUInt](#buffer), [BufferUIVec2](#buffer), [BufferUIVec3](#buffer), [BufferUIVec4](#buffer), [BufferUShort](#buffer), [BufferUSVec2](#buffer), [BufferUSVec3](#buffer), [BufferUSVec4](#buffer), [BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec3](#buffer), [BufferVec4](#buffer)) |  | 
`access` |  | `int` |  | `35002`
`direct_rendering` |  | `int` |  | `-1`
`immutable` |  | `int` |  | `0`


**Source**: [node_texture.c](/libnodegl/node_texture.c)

## Texture3D

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`format` |  | `int` |  | `6408`
`internal_format` |  | `int` |  | `6408`
`type` |  | `int` |  | `5121`
`width` |  | `int` |  | `0`
`height` |  | `int` |  | `0`
`depth` |  | `int` |  | `0`
`min_filter` |  | `int` |  | `9728`
`mag_filter` |  | `int` |  | `9728`
`wrap_s` |  | `int` |  | `33071`
`wrap_t` |  | `int` |  | `33071`
`wrap_r` |  | `int` |  | `33071`
`data_src` |  | `Node` ([AnimatedBufferFloat](#animatedbuffer), [AnimatedBufferVec2](#animatedbuffer), [AnimatedBufferVec3](#animatedbuffer), [AnimatedBufferVec4](#animatedbuffer), [BufferByte](#buffer), [BufferBVec2](#buffer), [BufferBVec3](#buffer), [BufferBVec4](#buffer), [BufferInt](#buffer), [BufferIVec2](#buffer), [BufferIVec3](#buffer), [BufferIVec4](#buffer), [BufferShort](#buffer), [BufferSVec2](#buffer), [BufferSVec3](#buffer), [BufferSVec4](#buffer), [BufferUByte](#buffer), [BufferUBVec2](#buffer), [BufferUBVec3](#buffer), [BufferUBVec4](#buffer), [BufferUInt](#buffer), [BufferUIVec2](#buffer), [BufferUIVec3](#buffer), [BufferUIVec4](#buffer), [BufferUShort](#buffer), [BufferUSVec2](#buffer), [BufferUSVec3](#buffer), [BufferUSVec4](#buffer), [BufferFloat](#buffer), [BufferVec2](#buffer), [BufferVec3](#buffer), [BufferVec4](#buffer)) |  | 
`access` |  | `int` |  | `35002`
`immutable` |  | `int` |  | `0`


**Source**: [node_texture.c](/libnodegl/node_texture.c)

## TimeRangeFilter

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | `Node` | time filtered scene | 
`ranges` |  | `NodeList` ([TimeRangeModeOnce](#timerangemodeonce), [TimeRangeModeNoop](#timerangemodenoop), [TimeRangeModeCont](#timerangemodecont)) | key frame time filtering events | 
`prefetch_time` |  | `double` | `child` is prefetched `prefetch_time` seconds in advance | `1`
`max_idle_time` |  | `double` | `child` will not be released if it is required in the next incoming `max_idle_time` seconds | `4`


**Source**: [node_timerangefilter.c](/libnodegl/node_timerangefilter.c)

## TimeRangeModeCont

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`start_time` | ✓ | `double` | starting time for the scene to be drawn | `0`


**Source**: [node_timerangemodes.c](/libnodegl/node_timerangemodes.c)

## TimeRangeModeNoop

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`start_time` | ✓ | `double` | starting time for the scene to stop being drawn | `0`


**Source**: [node_timerangemodes.c](/libnodegl/node_timerangemodes.c)

## TimeRangeModeOnce

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`start_time` | ✓ | `double` | starting time for the scene to be drawn once | `0`
`render_time` | ✓ | `double` | chosen time to draw | `0`


**Source**: [node_timerangemodes.c](/libnodegl/node_timerangemodes.c)

## Translate

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | `Node` | scene to translate | 
`vector` |  | `vec3` | translation vector | (`0`,`0`,`0`)
`anim` |  | `Node` ([AnimatedVec3](#animatedvec3)) | `vector` animation | 


**Source**: [node_translate.c](/libnodegl/node_translate.c)

## Triangle

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`edge0` | ✓ | `vec3` |  | (`0`,`0`,`0`)
`edge1` | ✓ | `vec3` |  | (`0`,`0`,`0`)
`edge2` | ✓ | `vec3` |  | (`0`,`0`,`0`)
`uv_edge0` |  | `vec2` |  | (`0`,`0`)
`uv_edge1` |  | `vec2` |  | (`0`,`1`)
`uv_edge2` |  | `vec2` |  | (`1`,`1`)


**Source**: [node_triangle.c](/libnodegl/node_triangle.c)

## UniformInt

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`value` |  | `int` |  | `0`


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)

## UniformMat4

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`value` |  | `mat4` |  | 
`transform` |  | `Node` ([Rotate](#rotate), [Translate](#translate), [Scale](#scale)) |  | 


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)

## UniformFloat

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`value` |  | `double` |  | `0`
`anim` |  | `Node` ([AnimatedFloat](#animatedfloat)) |  | 


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)

## UniformVec2

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`value` |  | `vec2` |  | (`0`,`0`)
`anim` |  | `Node` ([AnimatedVec2](#animatedvec2)) |  | 


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)

## UniformVec3

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`value` |  | `vec3` |  | (`0`,`0`,`0`)
`anim` |  | `Node` ([AnimatedVec3](#animatedvec3)) |  | 


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)

## UniformVec4

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`value` |  | `vec4` |  | (`0`,`0`,`0`,`0`)
`anim` |  | `Node` ([AnimatedVec4](#animatedvec4)) |  | 


**Source**: [node_uniform.c](/libnodegl/node_uniform.c)

