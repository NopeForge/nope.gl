libnodegl
=========
## AnimatedBuffer*

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`keyframes` |  | `NodeList` (`AnimKeyFrameBuffer`) |  | 


List of `AnimatedBuffer*` nodes:

- `AnimatedBufferFloat`
- `AnimatedBufferVec2`
- `AnimatedBufferVec3`
- `AnimatedBufferVec4`
## AnimatedFloat

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`keyframes` |  | `NodeList` (`AnimKeyFrameFloat`) |  | 


## AnimatedVec2

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`keyframes` |  | `NodeList` (`AnimKeyFrameVec2`) |  | 


## AnimatedVec3

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`keyframes` |  | `NodeList` (`AnimKeyFrameVec3`) |  | 


## AnimatedVec4

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`keyframes` |  | `NodeList` (`AnimKeyFrameVec4`) |  | 


## AnimKeyFrameFloat

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`time` | ✓ | `double` |  | `0`
`value` | ✓ | `double` |  | `0`
`easing` |  | `string` |  | 
`easing_args` |  | `doubleList` |  | 


## AnimKeyFrameVec2

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`time` | ✓ | `double` |  | `0`
`value` | ✓ | `vec2` |  | (`0`,`0`)
`easing` |  | `string` |  | 
`easing_args` |  | `doubleList` |  | 


## AnimKeyFrameVec3

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`time` | ✓ | `double` |  | `0`
`value` | ✓ | `vec3` |  | (`0`,`0`,`0`)
`easing` |  | `string` |  | 
`easing_args` |  | `doubleList` |  | 


## AnimKeyFrameVec4

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`time` | ✓ | `double` |  | `0`
`value` | ✓ | `vec4` |  | (`0`,`0`,`0`,`0`)
`easing` |  | `string` |  | 
`easing_args` |  | `doubleList` |  | 


## AnimKeyFrameBuffer

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`time` | ✓ | `double` |  | `0`
`data` |  | `data` |  | 
`easing` |  | `string` |  | 
`easing_args` |  | `doubleList` |  | 


## Buffer*

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`count` |  | `int` |  | `0`
`data` |  | `data` |  | 
`stride` |  | `int` |  | `0`
`target` |  | `int` |  | `34962`
`usage` |  | `int` |  | `35044`


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
`child` | ✓ | `Node` |  | 
`eye` |  | `vec3` |  | (`0`,`0`,`0`)
`center` |  | `vec3` |  | (`0`,`0`,`-1`)
`up` |  | `vec3` |  | (`0`,`1`,`0`)
`perspective` |  | `vec4` |  | (`0`,`0`,`0`,`0`)
`eye_transform` |  | `Node` (`Rotate`, `Translate`, `Scale`) |  | 
`center_transform` |  | `Node` (`Rotate`, `Translate`, `Scale`) |  | 
`up_transform` |  | `Node` (`Rotate`, `Translate`, `Scale`) |  | 
`fov_anim` |  | `Node` (`AnimatedFloat`) |  | 
`pipe_fd` |  | `int` |  | `0`
`pipe_width` |  | `int` |  | `0`
`pipe_height` |  | `int` |  | `0`


## Circle

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`radius` |  | `double` |  | `1`
`npoints` |  | `int` |  | `16`


## Compute

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`nb_group_x` | ✓ | `int` |  | `0`
`nb_group_y` | ✓ | `int` |  | `0`
`nb_group_z` | ✓ | `int` |  | `0`
`program` | ✓ | `Node` (`ComputeProgram`) |  | 
`textures` |  | `NodeDict` (`Texture2D`) |  | 
`uniforms` |  | `NodeDict` (`UniformFloat`, `UniformVec2`, `UniformVec3`, `UniformVec4`, `UniformInt`, `UniformMat4`) |  | 
`buffers` |  | `NodeDict` (`BufferFloat`, `BufferVec2`, `BufferVec3`, `BufferVec4`, `BufferInt`, `BufferIVec2`, `BufferIVec3`, `BufferIVec4`, `BufferUInt`, `BufferUIVec2`, `BufferUIVec3`, `BufferUIVec4`) |  | 


## ComputeProgram

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`compute` | ✓ | `string` |  | 


## FPS

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | `Node` |  | 
`measure_update` |  | `int` |  | `60`
`measure_draw` |  | `int` |  | `60`
`create_databuf` |  | `int` |  | `0`


## Geometry

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`vertices` | ✓ | `Node` (`BufferVec3`, `AnimatedBufferVec3`) |  | 
`uvcoords` |  | `Node` (`BufferFloat`, `BufferVec2`, `BufferVec3`, `AnimatedBufferFloat`, `AnimatedBufferVec2`, `AnimatedBufferVec3`) |  | 
`normals` |  | `Node` (`BufferVec3`, `AnimatedBufferVec3`) |  | 
`indices` |  | `Node` (`BufferUByte`, `BufferUInt`, `BufferUShort`) |  | 
`draw_mode` |  | `int` |  | `4`


## GraphicConfig

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | `Node` |  | 
`blend` |  | `Node` (`ConfigBlend`) |  | 
`colormask` |  | `Node` (`ConfigColorMask`) |  | 
`depth` |  | `Node` (`ConfigDepth`) |  | 
`polygonmode` |  | `Node` (`ConfigPolygonMode`) |  | 
`stencil` |  | `Node` (`ConfigStencil`) |  | 


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


## ConfigColorMask

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`enabled` | ✓ | `int` |  | `1`
`red` |  | `int` |  | `1`
`green` |  | `int` |  | `1`
`blue` |  | `int` |  | `1`
`alpha` |  | `int` |  | `1`


## ConfigPolygonMode

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`mode` | ✓ | `int` |  | `6914`


## ConfigDepth

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`enabled` | ✓ | `int` |  | `0`
`writemask` |  | `int` |  | `1`
`func` |  | `int` |  | `513`


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


## Group

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`children` |  | `NodeList` |  | 


## Identity

## Media

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`filename` | ✓ | `string` |  | 
`start` |  | `double` |  | `0`
`initial_seek` |  | `double` |  | `0`
`sxplayer_min_level` |  | `string` |  | 
`time_anim` |  | `Node` (`AnimatedFloat`) |  | 
`audio_tex` |  | `int` |  | `0`
`max_nb_packets` |  | `int` |  | `1`
`max_nb_frames` |  | `int` |  | `1`
`max_nb_sink` |  | `int` |  | `1`
`max_pixels` |  | `int` |  | `0`


## Program

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`vertex` |  | `string` |  | 
`fragment` |  | `string` |  | 


## Quad

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`corner` |  | `vec3` |  | (`-0.5`,`-0.5`,`0`)
`width` |  | `vec3` |  | (`1`,`0`,`0`)
`height` |  | `vec3` |  | (`0`,`1`,`0`)
`uv_corner` |  | `vec2` |  | (`0`,`0`)
`uv_width` |  | `vec2` |  | (`1`,`0`)
`uv_height` |  | `vec2` |  | (`0`,`1`)


## Render

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`geometry` | ✓ | `Node` (`Circle`, `Geometry`, `Quad`, `Triangle`) |  | 
`program` |  | `Node` (`Program`) |  | 
`textures` |  | `NodeDict` (`Texture2D`, `Texture3D`) |  | 
`uniforms` |  | `NodeDict` (`UniformFloat`, `UniformVec2`, `UniformVec3`, `UniformVec4`, `UniformInt`, `UniformMat4`) |  | 
`attributes` |  | `NodeDict` (`BufferFloat`, `BufferVec2`, `BufferVec3`, `BufferVec4`) |  | 
`buffers` |  | `NodeDict` (`BufferFloat`, `BufferVec2`, `BufferVec3`, `BufferVec4`, `BufferInt`, `BufferIVec2`, `BufferIVec3`, `BufferIVec4`, `BufferUInt`, `BufferUIVec2`, `BufferUIVec3`, `BufferUIVec4`) |  | 


## RenderToTexture

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | `Node` |  | 
`color_texture` | ✓ | `Node` (`Texture2D`) |  | 
`depth_texture` |  | `Node` (`Texture2D`) |  | 


## Rotate

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | `Node` |  | 
`angle` |  | `double` |  | `0`
`axis` |  | `vec3` |  | (`0`,`0`,`1`)
`anchor` |  | `vec3` |  | (`0`,`0`,`0`)
`anim` |  | `Node` (`AnimatedFloat`) |  | 


## Scale

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | `Node` |  | 
`factors` |  | `vec3` |  | (`0`,`0`,`0`)
`anchor` |  | `vec3` |  | (`0`,`0`,`0`)
`anim` |  | `Node` (`AnimatedVec3`) |  | 


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
`data_src` |  | `Node` (`Media`, `FPS`, `AnimatedBufferFloat`, `AnimatedBufferVec2`, `AnimatedBufferVec3`, `AnimatedBufferVec4`, `BufferByte`, `BufferBVec2`, `BufferBVec3`, `BufferBVec4`, `BufferInt`, `BufferIVec2`, `BufferIVec3`, `BufferIVec4`, `BufferShort`, `BufferSVec2`, `BufferSVec3`, `BufferSVec4`, `BufferUByte`, `BufferUBVec2`, `BufferUBVec3`, `BufferUBVec4`, `BufferUInt`, `BufferUIVec2`, `BufferUIVec3`, `BufferUIVec4`, `BufferUShort`, `BufferUSVec2`, `BufferUSVec3`, `BufferUSVec4`, `BufferFloat`, `BufferVec2`, `BufferVec3`, `BufferVec4`) |  | 
`access` |  | `int` |  | `35002`
`direct_rendering` |  | `int` |  | `-1`
`immutable` |  | `int` |  | `0`


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
`data_src` |  | `Node` (`AnimatedBufferFloat`, `AnimatedBufferVec2`, `AnimatedBufferVec3`, `AnimatedBufferVec4`, `BufferByte`, `BufferBVec2`, `BufferBVec3`, `BufferBVec4`, `BufferInt`, `BufferIVec2`, `BufferIVec3`, `BufferIVec4`, `BufferShort`, `BufferSVec2`, `BufferSVec3`, `BufferSVec4`, `BufferUByte`, `BufferUBVec2`, `BufferUBVec3`, `BufferUBVec4`, `BufferUInt`, `BufferUIVec2`, `BufferUIVec3`, `BufferUIVec4`, `BufferUShort`, `BufferUSVec2`, `BufferUSVec3`, `BufferUSVec4`, `BufferFloat`, `BufferVec2`, `BufferVec3`, `BufferVec4`) |  | 
`access` |  | `int` |  | `35002`
`immutable` |  | `int` |  | `0`


## TimeRangeFilter

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | `Node` |  | 
`ranges` |  | `NodeList` (`TimeRangeModeOnce`, `TimeRangeModeNoop`, `TimeRangeModeCont`) |  | 
`prefetch_time` |  | `double` |  | `1`
`max_idle_time` |  | `double` |  | `4`


## TimeRangeModeCont

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`start_time` | ✓ | `double` |  | `0`


## TimeRangeModeNoop

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`start_time` | ✓ | `double` |  | `0`


## TimeRangeModeOnce

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`start_time` | ✓ | `double` |  | `0`
`render_time` | ✓ | `double` |  | `0`


## Translate

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`child` | ✓ | `Node` |  | 
`vector` |  | `vec3` |  | (`0`,`0`,`0`)
`anim` |  | `Node` (`AnimatedVec3`) |  | 


## Triangle

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`edge0` | ✓ | `vec3` |  | (`0`,`0`,`0`)
`edge1` | ✓ | `vec3` |  | (`0`,`0`,`0`)
`edge2` | ✓ | `vec3` |  | (`0`,`0`,`0`)
`uv_edge0` |  | `vec2` |  | (`0`,`0`)
`uv_edge1` |  | `vec2` |  | (`0`,`1`)
`uv_edge2` |  | `vec2` |  | (`1`,`1`)


## UniformInt

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`value` |  | `int` |  | `0`


## UniformMat4

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`value` |  | `mat4` |  | 
`transform` |  | `Node` (`Rotate`, `Translate`, `Scale`) |  | 


## UniformFloat

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`value` |  | `double` |  | `0`
`anim` |  | `Node` (`AnimatedFloat`) |  | 


## UniformVec2

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`value` |  | `vec2` |  | (`0`,`0`)
`anim` |  | `Node` (`AnimatedVec2`) |  | 


## UniformVec3

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`value` |  | `vec3` |  | (`0`,`0`,`0`)
`anim` |  | `Node` (`AnimatedVec3`) |  | 


## UniformVec4

Parameter | Ctor. | Type | Description | Default
--------- | :---: | ---- | ----------- | :-----:
`value` |  | `vec4` |  | (`0`,`0`,`0`,`0`)
`anim` |  | `Node` (`AnimatedVec4`) |  | 


